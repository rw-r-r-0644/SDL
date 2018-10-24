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

#include "../../video/wiiu/SDL_wiiuvideo.h"
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include "SDL_render_wiiu.h"

#include <gx2/context.h>
#include <gx2/texture.h>
#include <gx2/sampler.h>
#include <gx2/mem.h>

#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int WIIU_SDL_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    WIIU_TextureData *tdata = (WIIU_TextureData *) SDL_calloc(1, sizeof(*tdata));
    if (!tdata)
        return SDL_OutOfMemory();

    // Setup sampler
    GX2InitSampler(&tdata->sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    tdata->texture.surface.width = TextureNextPow2(texture->w);
    tdata->texture.surface.height = TextureNextPow2(texture->h);
    tdata->texture.surface.format = PixelFormatToWIIUFMT(texture->format);
    tdata->texture.surface.depth = 1; //?
    tdata->texture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    tdata->texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    tdata->texture.surface.use = GX2_SURFACE_USE_TEXTURE;
    tdata->texture.surface.mipLevels = 1;
    tdata->texture.viewNumMips = 1;
    tdata->texture.viewNumSlices = 1;
    tdata->texture.compMap = 0x00010203;

    GX2CalcSurfaceSizeAndAlignment(&tdata->texture.surface);
    tdata->texture.surface.image = memalign(tdata->texture.surface.alignment, tdata->texture.surface.imageSize);
    if(!tdata->texture.surface.image)
    {
        SDL_free(tdata);
        return SDL_OutOfMemory();
    }
    texture->driverdata = tdata;

    return 0;
}

// Somewhat adapted from SDL_render.c: SDL_LockTextureNative
// The app basically wants a pointer to a particular rectangle as well as
// write access to it. We can do that without any special graphics code
int WIIU_SDL_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * rect, void **pixels, int *pitch)
{
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;

    // Calculate pointer to first pixel in rect
    *pixels = (void *) ((Uint8 *) tdata->texture.surface.image +
                        rect->y * tdata->texture.surface.pitch +
                        rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = tdata->texture.surface.pitch;

    // Not sure we even need to bother keeping track of this
    texture->locked_rect = *rect;
    return 0;
}

void WIIU_SDL_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;
    // TODO check this is actually needed
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE,
        tdata->texture.surface.image, tdata->texture.surface.imageSize);
}

int WIIU_SDL_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                           const SDL_Rect * rect, const void *pixels, int pitch)
{
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;
    Uint32 BytesPerPixel;
    Uint8 *src, *dst;
    int row;
    size_t length;

    BytesPerPixel = PixelFormatByteSizeWIIU(texture->format);

    src = (Uint8 *) pixels;
    dst = (Uint8 *) tdata->texture.surface.image +
                        rect->y * tdata->texture.surface.pitch +
                        rect->x * BytesPerPixel;
    length = rect->w * BytesPerPixel;
    for (row = 0; row < rect->h; ++row) {
        SDL_memcpy(dst, src, length);
        src += pitch;
        dst += tdata->texture.surface.pitch;
    }

    return 0;
}

void WIIU_SDL_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;
    free(tdata->texture.surface.image);
    SDL_free(tdata);
}

#endif //SDL_VIDEO_RENDER_WIIU
