# AreaResize

AreaResize is an area (average) downscaler plugin for AviSynth.  Downscaling in RGB32/RGB24 is also gamma corrected.

## Usage

```
AreaResize(int width, int height)
```

## Supported Colorspaces

- 8-bit Planar (YV24/YV16/YV12/YV411/Y8) (YUY2 is not supported; use YV12)
- 8-bit Interleaved (RGB32/RGB24) (gamma corrected)

## Features

- Gamma corrected downscaling to RGB32/RGB24 (gamma scale of 2.2)
- Significant performance increase due to multithreading and optimized code

# Credits

Based on **[Oka Motofumi](https://github.com/chikuzen)**'s version of **[AreaResize](https://github.com/chikuzen/AreaResize)**.