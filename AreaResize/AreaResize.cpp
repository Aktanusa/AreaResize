/*
    AreaResize.dll

    Copyright (C) 2012 Oka Motofumi(chikuzen.mo at gmail dot com)

    author : Oka Motofumi

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <windows.h>
#include <math.h>
#include <ppl.h>
#include "avisynth.h"

#define RGB_PIXEL_RANGE 256
#define RGB_PIXEL_RANGE_EXTENDED 25501
#define GAMMA 2.2
#define DOUBLE_ROUND_MAGIC_NUMBER 6755399441055744.0

extern const std::string version = "1.0";

typedef struct {
    unsigned short src_width;
	unsigned short src_height;
	unsigned short target_width;
	unsigned short target_height;
	unsigned short num_h;
	unsigned short den_h;
	unsigned short num_v;
	unsigned short den_v;
} params_t;

class AreaResize : public GenericVideoFilter {
public:
    AreaResize(PClip _child, int target_width, int target_height, IScriptEnvironment* env);
    ~AreaResize();
    PVideoFrame _stdcall GetFrame(int n, IScriptEnvironment* env);
private:
	double linear_LUT[RGB_PIXEL_RANGE];
	BYTE gamma_LUT[RGB_PIXEL_RANGE_EXTENDED];

	static const int num_plane = 3;
	params_t params[num_plane];

	BYTE plane_size;

	BYTE* buff;

	bool(AreaResize::*ResizeHorizontal)(BYTE*, const BYTE*, const int, const params_t*);
	bool(AreaResize::*ResizeVertical)(BYTE*, const int, const BYTE*, const int, const params_t*);

	int gcd(int x, int y);
	bool ResizeHorizontalPlanar(BYTE* dstp, const BYTE* srcp, const int src_pitch, const params_t* params);
	bool ResizeVerticalPlanar(BYTE* dstp, const int dst_pitch, const BYTE* srcp, const int src_pitch, const params_t* params);
	bool ResizeHorizontalRGB(BYTE* dstp, const BYTE* srcp, const int src_pitch, const params_t* params);
	bool ResizeVerticalRGB(BYTE* dstp, const int dst_pitch, const BYTE* srcp, const int src_pitch, const params_t* params);
};

int AreaResize::gcd(int x, int y) {
	int m = x % y;
	return m == 0 ? y : gcd(y, m);
}

AreaResize::AreaResize(PClip _child, int target_width, int target_height, IScriptEnvironment* env) : GenericVideoFilter(_child) {
	for (int i = 0; i < RGB_PIXEL_RANGE; i++) {
		linear_LUT[i] = pow(((double)i / ((double)RGB_PIXEL_RANGE - 1.0)), GAMMA) * ((double)RGB_PIXEL_RANGE - 1.0);
		gamma_LUT[i] = (BYTE)round(pow((((double)i / 100.0) / ((double)RGB_PIXEL_RANGE - 1.0)), (1.0 / GAMMA)) * ((double)RGB_PIXEL_RANGE - 1.0));
	}
	for (int i = RGB_PIXEL_RANGE; i < RGB_PIXEL_RANGE_EXTENDED; i++) {
		gamma_LUT[i] = (BYTE)round(pow((((double)i / 100.0) / ((double)RGB_PIXEL_RANGE - 1.0)), (1.0 / GAMMA)) * ((double)RGB_PIXEL_RANGE - 1.0));
	}

	plane_size = vi.IsRGB32() ? 4 : vi.IsRGB24() ? 3 : 1;
	const BYTE ps = plane_size;

	buff = NULL;
    if (target_width != vi.width) {
        buff = (BYTE *)malloc(sizeof(double) * target_width * vi.height * ps);
		if (!buff) {
            env->ThrowError("AreaResize: Out of memory");
        }
    }

    for (int i = 0; i < num_plane; i++) {
        params[i].src_width     = i ? vi.width / vi.SubsampleH() : vi.width;
        params[i].src_height    = i ? vi.height / vi.SubsampleV() : vi.height;
        params[i].target_width  = i ? target_width / vi.SubsampleH() : target_width;
        params[i].target_height = i ? target_height / vi.SubsampleV() : target_height;
		int gcd_h = gcd(params[i].src_width, params[i].target_width);
		int gcd_v = gcd(params[i].src_height, params[i].target_height);
		params[i].num_h = params[i].target_width / gcd_h;
		params[i].den_h = params[i].src_width / gcd_h;
		params[i].num_v = params[i].target_height / gcd_v;
		params[i].den_v = params[i].src_height / gcd_v;
	}

    vi.width = target_width;
    vi.height = target_height;

    if (ps > 1) {
        ResizeHorizontal = &AreaResize::ResizeHorizontalRGB;
        ResizeVertical = &AreaResize::ResizeVerticalRGB;
    } else {
        ResizeHorizontal = &AreaResize::ResizeHorizontalPlanar;
        ResizeVertical = &AreaResize::ResizeVerticalPlanar;
    }
}

AreaResize::~AreaResize() {
    if (buff) {
        free(buff);
    }
}

bool AreaResize::ResizeHorizontalPlanar(BYTE* dstp, const BYTE* srcp, const int src_pitch, const params_t* params) {
	const unsigned short src_height = params->src_height;
	const unsigned short target_width = params->target_width;
	const unsigned short num = params->num_h;
	const unsigned short den = params->den_h;
	const double invert_den = 1.0 / (double)den;

	Concurrency::parallel_for(0, (int)src_height, [&](unsigned short curPixel) {
		//for (int curPixel = 0; curPixel < src_height; curPixel++) {
		const BYTE* curSrcp = srcp + (src_pitch * curPixel);
		//BYTE* curBuff = dstp + (target_width * curPixel);
		BYTE* curBuff = dstp + curPixel;

		unsigned short count_num = num;
		unsigned int index_src = 0;
		for (unsigned short index_value = target_width; index_value--; ) {
			unsigned int pixel = 0;
			unsigned short count_den = den;
			while (count_den > 0) {
				short left = count_num - count_den;
				unsigned short partial;
				if (left <= 0) {
					partial = count_num;
					count_den = (int)abs(left);
					count_num = num;
				}
				else {
					count_num = left;
					partial = count_den;
					count_den = 0;
				}
				pixel += (curSrcp[index_src] * partial);
				if (left <= 0) {
					index_src++;
				}
			}
			const unsigned int index = (target_width - index_value - 1) * src_height;
			//const unsigned int index = (target_width - index_value - 1);
			double pixelConvert = ((double)pixel * invert_den) + DOUBLE_ROUND_MAGIC_NUMBER;
			const BYTE pixelValue = (BYTE)reinterpret_cast<int&>(pixelConvert);
			curBuff[index] = pixelValue;
		}
	});
	//}

	return true;
}

bool AreaResize::ResizeVerticalPlanar(BYTE* dstp, const int dst_pitch, const BYTE* srcp, const int src_pitch, const params_t* params) {
	const unsigned short target_width = params->target_width;
	const unsigned short target_height = params->target_height;
	const unsigned short src_height = params->src_height;
	const unsigned short num = params->num_v;
	const unsigned short den = params->den_v;
	const double invert_den = 1.0 / (double)den;

	Concurrency::parallel_for(0, (int)target_width, [&](unsigned short curPixel) {
		//for (int curPixel = 0; curPixel < target_width; curPixel++) {
		//const BYTE* curSrcp = srcp + curPixel;
		const BYTE* curSrcp = srcp + (src_height * curPixel);
		BYTE* curDstp = dstp + curPixel;

		unsigned short count_num = num;
		unsigned int index_src = 0;
		for (unsigned short index_value = target_height; index_value--; ) {
			unsigned int pixel = 0;
			unsigned short count_den = den;
			while (count_den > 0) {
				short left = count_num - count_den;
				unsigned short partial;
				if (left <= 0) {
					partial = count_num;
					count_den = (int)abs(left);
					count_num = num;
				}
				else {
					count_num = left;
					partial = count_den;
					count_den = 0;
				}
				pixel += curSrcp[index_src] * partial;
				if (left <= 0) {
					//index_src += src_pitch;
					index_src++;
				}
			}
			const unsigned int index = (target_height - index_value - 1) * dst_pitch;
			double pixelConvert = ((double)pixel * invert_den) + DOUBLE_ROUND_MAGIC_NUMBER;
			const BYTE pixelValue = (BYTE)reinterpret_cast<int&>(pixelConvert);
			curDstp[index] = pixelValue;
		}
	});
	//}

	return true;
}

bool AreaResize::ResizeHorizontalRGB(BYTE* dstp, const BYTE* srcp, const int src_pitch, const params_t* params) {
	const unsigned short src_height = params->src_height;
	const unsigned short target_width = params->target_width;
	const unsigned short num = params->num_h;
	const unsigned short den = params->den_h;
	const double invert_den_hun = 100.0 / (double)den;
	const BYTE ps = plane_size;
	const double* local_lin_LUT = linear_LUT;
	const BYTE* local_gam_LUT = gamma_LUT;

	Concurrency::parallel_for(0, (int)src_height, [&](unsigned short curPixel) {
	//for (int curPixel = 0; curPixel < src_height; curPixel++) {
		const BYTE* curSrcp = srcp + (src_pitch * curPixel);
		//BYTE* curBuff = dstp + (target_width * curPixel * ps);
		BYTE* curBuff = dstp + (ps * curPixel);

		unsigned short count_num = num;
		unsigned int index_src = 0;
		for (unsigned short index_value = target_width; index_value--; ) {
			double blue = 0.0;
			double green = 0.0;
			double red = 0.0;
			double alpha = 0.0;
			unsigned short count_den = den;
			while (count_den > 0) {
				short left = count_num - count_den;
				double partial;
				if (left <= 0) {
					partial = count_num;
					count_den = (int)abs(left);
					count_num = num;
				}
				else {
					count_num = left;
					partial = count_den;
					count_den = 0;
				}
				switch (ps) {
					case 3:
						// Repeated for SIMD
						blue += ((local_lin_LUT[curSrcp[index_src]]) * partial);
						green += ((local_lin_LUT[curSrcp[index_src + 1]]) * partial);
						red += ((local_lin_LUT[curSrcp[index_src + 2]]) * partial);
						break;
					case 4:
						blue += ((local_lin_LUT[curSrcp[index_src]]) * partial);
						green += ((local_lin_LUT[curSrcp[index_src + 1]]) * partial);
						red += ((local_lin_LUT[curSrcp[index_src + 2]]) * partial);
						alpha += ((local_lin_LUT[curSrcp[index_src + 3]]) * partial);
						break;
				}
				if (left <= 0) {
					index_src += ps;
				}
			}
			const unsigned int index = (target_width - index_value - 1) * src_height * ps;
			//const unsigned int index = (target_width - index_value - 1) * ps;
			BYTE blueValue, greenValue, redValue, alphaValue;
			switch (ps) {
				case 3:
					// Repeated for SIMD
					blue = (blue * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
					green = (green * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
					red = (red * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;

					blueValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(blue)];
					greenValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(green)];
					redValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(red)];

					curBuff[index] = blueValue;
					curBuff[index + 1] = greenValue;
					curBuff[index + 2] = redValue;
					break;
				case 4:
					blue = (blue * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
					green = (green * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
					red = (red * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
					alpha = (alpha * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;

					blueValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(blue)];
					greenValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(green)];
					redValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(red)];
					alphaValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(alpha)];

					curBuff[index] = blueValue;
					curBuff[index + 1] = greenValue;
					curBuff[index + 2] = redValue;
					curBuff[index + 3] = alphaValue;
					break;
			}
		}
	});
	//}

	return true;
}

bool AreaResize::ResizeVerticalRGB(BYTE* dstp, const int dst_pitch, const BYTE* srcp, const int src_pitch, const params_t* params) {
	const unsigned short target_width = params->target_width;
	const unsigned short target_height = params->target_height;
	const unsigned short src_height = params->src_height;
	const unsigned short num = params->num_v;
	const unsigned short den = params->den_v;
	const double invert_den_hun = 100.0 / (double)den;
	const BYTE ps = plane_size;
	const double* local_lin_LUT = linear_LUT;
	const BYTE* local_gam_LUT = gamma_LUT;

	Concurrency::parallel_for(0, (int)target_width, [&](unsigned short curPixel) {
	//for (int curPixel = 0; curPixel < target_width; curPixel++) {
		//const BYTE* curSrcp = srcp + (ps * curPixel);
		const BYTE* curSrcp = srcp + (src_height * curPixel * ps);
		BYTE* curDstp = dstp + (ps * curPixel);

		unsigned short count_num = num;
		unsigned int index_src = 0;
		for (unsigned short index_value = target_height; index_value--; ) {
			double blue = 0.0;
			double green = 0.0;
			double red = 0.0;
			double alpha = 0.0;
			unsigned short count_den = den;
			while (count_den > 0) {
				short left = count_num - count_den;
				double partial;
				if (left <= 0) {
					partial = count_num;
					count_den = (int)abs(left);
					count_num = num;
				}
				else {
					count_num = left;
					partial = count_den;
					count_den = 0;
				}
				switch (ps) {
				case 3:
					// Repeated for SIMD
					blue += ((local_lin_LUT[curSrcp[index_src]]) * partial);
					green += ((local_lin_LUT[curSrcp[index_src + 1]]) * partial);
					red += ((local_lin_LUT[curSrcp[index_src + 2]]) * partial);
					break;
				case 4:
					blue += ((local_lin_LUT[curSrcp[index_src]]) * partial);
					green += ((local_lin_LUT[curSrcp[index_src + 1]]) * partial);
					red += ((local_lin_LUT[curSrcp[index_src + 2]]) * partial);
					alpha += ((local_lin_LUT[curSrcp[index_src + 3]]) * partial);
					break;
				}
				if (left <= 0) {
					//index_src += src_pitch;
					index_src += ps;
				}
			}
			const unsigned int index = (target_height - index_value - 1) * dst_pitch;
			BYTE blueValue, greenValue, redValue, alphaValue;
			switch (ps) {
			case 3:
				// Repeated for SIMD
				blue = (blue * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
				green = (green * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
				red = (red * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;

				blueValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(blue)];
				greenValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(green)];
				redValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(red)];

				curDstp[index] = blueValue;
				curDstp[index + 1] = greenValue;
				curDstp[index + 2] = redValue;
				break;
			case 4:
				blue = (blue * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
				green = (green * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
				red = (red * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;
				alpha = (alpha * invert_den_hun) + DOUBLE_ROUND_MAGIC_NUMBER;

				blueValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(blue)];
				greenValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(green)];
				redValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(red)];
				alphaValue = (BYTE)local_gam_LUT[reinterpret_cast<int&>(alpha)];

				curDstp[index] = blueValue;
				curDstp[index + 1] = greenValue;
				curDstp[index + 2] = redValue;
				curDstp[index + 3] = alphaValue;
				break;
			}
		}
	});
	//}

	return true;
}

PVideoFrame AreaResize::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);
    if (params[0].src_width == params[0].target_width &&
        params[0].src_height == params[0].target_height) {
        return src;
    }

    PVideoFrame dst = env->NewVideoFrame(vi);

    int plane[] = {PLANAR_Y, PLANAR_U, PLANAR_V};
    for (int i = 0, time = vi.IsInterleaved() ? 1 : 3; i < time; i++) {
        const BYTE* srcp = src->GetReadPtr(plane[i]);
        int src_pitch = src->GetPitch(plane[i]);

        const BYTE* resized_h;
		if (params[i].src_width == params[i].target_width) {
            resized_h = srcp;
        } else {
            if (!(this->*(this->ResizeHorizontal))(buff, srcp, src_pitch, &params[i])) {
                return dst;
            }
            resized_h = buff;
            src_pitch = dst->GetRowSize(plane[i]);
        }

        BYTE* dstp = dst->GetWritePtr(plane[i]);
        int dst_pitch = dst->GetPitch(plane[i]);
        if (params[i].src_height == params[i].target_height) {
            env->BitBlt(dstp, dst_pitch, resized_h, src_pitch, dst->GetRowSize(plane[i]), dst->GetHeight(plane[i]));
            continue;
        }

        if (!(this->*(this->ResizeVertical))(dstp, dst_pitch, resized_h, src_pitch, &params[i])) {
            return dst;
        }
    }

    return dst;
}

AVSValue __cdecl CreateAreaResize(AVSValue args, void* user_data, IScriptEnvironment* env) {
    PClip clip = args[0].AsClip();
    int target_width = args[1].AsInt();
    int target_height = args[2].AsInt();

    if (target_width < 1 || target_height < 1) {
        env->ThrowError("AreaResize: Target width/height must be 1 or higher.");
    }

    const VideoInfo& vi = clip->GetVideoInfo();
    if (vi.IsYUY2()) {
        env->ThrowError("AreaResize: Unsupported colorspace(YUY2).");
    }
    if (vi.IsYV411() && target_width & 3) {
        env->ThrowError("AreaResize: Target width requires mod 4.");
    }
    if ((vi.IsYV16() || vi.IsYV12()) && target_width & 1) {
        env->ThrowError("AreaResize: Target width requires mod 2.");
    }
    if (vi.IsYV12() && target_height & 1) {
        env->ThrowError("AreaResize: Target height requires mod 2.");
    }
    if (vi.width < target_width || vi.height < target_height) {
        env->ThrowError("AreaResize: This filter is only for downscale.");
    }

    return new AreaResize(clip, target_width, target_height, env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env) {
    env->AddFunction("AreaResize", "c[width]i[height]i", CreateAreaResize, 0);
	std::string result = "AreaResize for AviSynth " + version;
    return result.c_str();
}
