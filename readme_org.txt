AreaResize.dll version 0.1.0

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


What's this?
	an area-average resizer plugin for avisynth


How to use?

	LoadPlugin("AreaResize.dll")
	AVISource("video.avi")
	AreaResize(int target_width, int target_height)

	note: This filter is only for down scale.
	      supported colorspaces are YV12/YV16/YV24/YV411/Y8/RGB24/RGB32.
	      (YUY2 is unsupported. Use YV16)

requirement
	WindowsXPSP3/Vista/7
	AviSynth2.58 or 2.6x
	Microsoft Visual C++ 2010 Redistributable Package

sourcecode
	https://github.com/chikuzen/AreaResize