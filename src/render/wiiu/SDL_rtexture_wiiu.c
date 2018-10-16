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

#if SDL_VIDEO_RENDER_WIIU

#include "../../video/wiiu/wiiuvideo.h"
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include "SDL_render_wiiu.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int
WIIU_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GX2Texture *wiiu_tex = (GX2Texture*) SDL_calloc(1, sizeof(*wiiu_tex));
    if (!wiiu_tex)
        return SDL_OutOfMemory();

    wiiu_tex->surface.width = TextureNextPow2(texture->w);
    wiiu_tex->surface.height = TextureNextPow2(texture->h);
    wiiu_tex->surface.format = PixelFormatToWIIUFMT(texture->format);
    wiiu_tex->surface.depth = 1; //?
    wiiu_tex->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    wiiu_tex->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    wiiu_tex->surface.use = GX2_SURFACE_USE_TEXTURE;
    wiiu_tex->surface.mipLevels = 1;
    wiiu_tex->viewNumMips = 1;
    wiiu_tex->viewNumSlices = 1;
    wiiu_tex->compMap = 0x00010203;

    GX2CalcSurfaceSizeAndAlignment(&wiiu_tex->surface);
    wiiu_tex->surface.image = memalign(wiiu_tex->surface.alignment, wiiu_tex->surface.imageSize);
    if(!wiiu_tex->surface.image)
    {
        SDL_free(wiiu_tex);
        return SDL_OutOfMemory();
    }
    texture->driverdata = wiiu_tex;

    return 0;
}

// Somewhat adapted from SDL_render.c: SDL_LockTextureNative
// The app basically wants a pointer to a particular rectangle as well as
// write access to it. We can do that without any special graphics code
static int WIIU_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect, void **pixels, int *pitch)
{
    GX2Texture *wiiu_tex = (GX2Texture *) texture->driverdata;

    // Calculate pointer to first pixel in rect
    *pixels = (void *) ((Uint8 *) wiiu_tex->surface.image +
                        rect->y * wiiu_tex->surface.pitch +
                        rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = wiiu_tex->surface.pitch;

    // Not sure we even need to bother keeping track of this
    texture->locked_rect = *rect;
    return 0;
}

static void WIIU_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GX2Texture *wiiu_tex = (GX2Texture *) texture->driverdata;
    // TODO check this is actually needed
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE,
        wiiu_tex->surface.image, wiiu_tex->surface.imageSize);
}

static Uint32
TextureNextPow2(Uint32 w)
{
    if(w == 0)
        return 0;
    Uint32 n = 2;
    while(w > n)
        n <<= 1;
    return n;
}

//TODO: This could return a compMap to support stuff like ARGB or ABGR
static GX2SurfaceFormat
PixelFormatToWIIUFMT(Uint32 format)
{
    switch (format) {
        case SDL_PIXELFORMAT_RGBA8888:
            return GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        case SDL_PIXELFORMAT_RGBA4444:
            return GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
        case SDL_PIXELFORMAT_ABGR1555:
            return GX2_SURFACE_FORMAT_UNORM_A1_B5_G5_R5;
        case SDL_PIXELFORMAT_RGBA5551:
            return GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
        case SDL_PIXELFORMAT_RGB565:
            return GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
        default:
            return GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    }
    return GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
}

#endif //SDL_VIDEO_RENDER_WIIU
