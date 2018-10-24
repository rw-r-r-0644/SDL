/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2018 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2018 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#ifndef _WIIU_shaders_h
#define _WIIU_shaders_h

#include <whb/gfx.h>
#include <gx2/shaders.h>

extern unsigned char wiiuTextureShaderData[];
extern unsigned char wiiuColorShaderData[];

WHBGfxShaderGroup wiiuTextureShader;
int wiiuTextureShaderInit = 0;

static inline void wiiuInitTextureShader() {
    if (!wiiuTextureShaderInit) {
        WHBGfxLoadGFDShaderGroup(&wiiuTextureShader, 0, wiiuTextureShaderData);
	    WHBGfxInitShaderAttribute(&wiiuTextureShader, "a_position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
	    WHBGfxInitShaderAttribute(&wiiuTextureShader, "a_texCoordIn", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
        WHBGfxInitFetchShader(&wiiuTextureShader);
    }
    wiiuTextureShaderInit++;
}

static inline void wiiuFreeTextureShader() {
    if (wiiuTextureShaderInit)
        if (!--wiiuTextureShaderInit)
            WHBGfxFreeShaderGroup(&wiiuTextureShader);
}

static inline void wiiuSetTextureShader() {
    GX2SetFetchShader(&wiiuTextureShader.fetchShader);
    GX2SetVertexShader(wiiuTextureShader.vertexShader);
    GX2SetPixelShader(wiiuTextureShader.pixelShader);
}

WHBGfxShaderGroup wiiuColorShader;
int wiiuColorShaderInit = 0;

static inline void wiiuInitColorShader() {
    if (!wiiuColorShaderInit) {
        WHBGfxLoadGFDShaderGroup(&wiiuColorShader, 0, wiiuColorShaderData);
        WHBGfxInitShaderAttribute(&wiiuColorShader, "a_position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
        WHBGfxInitFetchShader(&wiiuColorShader);
    }
    wiiuColorShaderInit++;
}

static inline void wiiuFreeColorShader() {
    if (wiiuColorShaderInit)
        if (!--wiiuColorShaderInit)
            WHBGfxFreeShaderGroup(&wiiuColorShader);
}

static inline void wiiuSetColorShader() {
    GX2SetFetchShader(&wiiuColorShader.fetchShader);
    GX2SetVertexShader(wiiuColorShader.vertexShader);
    GX2SetPixelShader(wiiuColorShader.pixelShader);
}

#endif //_WIIU_shaders_h
