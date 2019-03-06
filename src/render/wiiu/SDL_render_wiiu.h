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

#ifndef SDL_render_wiiu_h
#define SDL_render_wiiu_h

#include "../SDL_sysrender.h"
#include "SDL_pixels.h"
#include <gx2/context.h>
#include <gx2/sampler.h>
#include <gx2r/buffer.h>

typedef struct
{
    void *next;
    GX2RBuffer buffer;
} WIIU_RenderAllocData;

//Driver internal data structures
typedef struct
{
    GX2ColorBuffer cbuf;
    GX2ContextState *ctx;
    WIIU_RenderAllocData *listfree;
    float u_viewSize[4];
    SDL_Texture windowTex;
} WIIU_RenderData;

typedef struct
{
    GX2Sampler sampler;
    GX2Texture texture;
    float u_texSize[4];
} WIIU_TextureData;

static inline GX2RBuffer* WIIU_AllocRenderData(WIIU_RenderData *r, GX2RBuffer buffer) {
    WIIU_RenderAllocData *rdata = SDL_malloc(sizeof(WIIU_RenderAllocData));

    rdata->buffer = buffer;
    if (!GX2RCreateBuffer(&rdata->buffer)) {
        SDL_free(rdata);
        return 0;
    }

    rdata->next = r->listfree;
    r->listfree = rdata;
    return &rdata->buffer;
}

static inline void WIIU_FreeRenderData(WIIU_RenderData *r) {
    while (r->listfree) {
        WIIU_RenderAllocData *ptr = r->listfree;
        r->listfree = r->listfree->next;
        GX2RDestroyBufferEx(&ptr->buffer, 0);
        SDL_free(ptr);
    }
}

//SDL_render API implementation
SDL_Renderer *WIIU_SDL_CreateRenderer(SDL_Window * window, Uint32 flags);
void WIIU_SDL_WindowEvent(SDL_Renderer * renderer,
                             const SDL_WindowEvent *event);
int WIIU_SDL_GetOutputSize(SDL_Renderer * renderer, int *w, int *h);
int WIIU_SDL_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
// SDL changes colour/alpha/blend values internally, this is just to notify us.
// We don't care yet. TODO: could update GX2RBuffers less frequently with these?
/*int WIIU_SDL_SetTextureColorMod(SDL_Renderer * renderer,
                                SDL_Texture * texture);
int WIIU_SDL_SetTextureAlphaMod(SDL_Renderer * renderer,
                                SDL_Texture * texture);
int WIIU_SDL_SetTextureBlendMode(SDL_Renderer * renderer,
                                 SDL_Texture * texture);*/
int WIIU_SDL_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                       const SDL_Rect * rect, const void *pixels,
                       int pitch);
int WIIU_SDL_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * rect, void **pixels, int *pitch);
void WIIU_SDL_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
int WIIU_SDL_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture);
int WIIU_SDL_UpdateViewport(SDL_Renderer * renderer);
int WIIU_SDL_UpdateClipRect(SDL_Renderer * renderer);
int WIIU_SDL_RenderClear(SDL_Renderer * renderer);
int WIIU_SDL_RenderDrawPoints(SDL_Renderer * renderer,
                          const SDL_FPoint * points, int count);
int WIIU_SDL_RenderDrawLines(SDL_Renderer * renderer,
                         const SDL_FPoint * points, int count);
int WIIU_SDL_RenderFillRects(SDL_Renderer * renderer,
                         const SDL_FRect * rects, int count);
int WIIU_SDL_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                    const SDL_Rect * srcrect, const SDL_FRect * dstrect);
int WIIU_SDL_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                      const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                      const double angle, const SDL_FPoint * center, const SDL_RendererFlip flip);
int WIIU_SDL_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                          Uint32 format, void * pixels, int pitch);
void WIIU_SDL_RenderPresent(SDL_Renderer * renderer);
void WIIU_SDL_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture);
void WIIU_SDL_DestroyRenderer(SDL_Renderer * renderer);

//Driver internal functions
void WIIU_SDL_CreateWindowTex(SDL_Renderer * renderer, SDL_Window * window);

//Utility/helper functions
static inline Uint32 TextureNextPow2(Uint32 w) {
    Uint32 n = 2;
    if(w == 0)
        return 0;
    while(w > n)
        n <<= 1;
    return n;
}

typedef struct {
    union { float x, r; };
    union { float y, g; };
} WIIUVec2;
typedef struct {
    union { float x, r; };
    union { float y, g; };
    union { float z, b; };
} WIIUVec3;
typedef struct {
    union { float x, r; };
    union { float y, g; };
    union { float z, b; };
    union { float w, a; };
} WIIUVec4;

typedef struct WIIUPixFmt {
    GX2SurfaceFormat fmt;
    uint32_t compMap;
} WIIUPixFmt;

static inline WIIUPixFmt SDLFormatToWIIUFormat(Uint32 format) {
    WIIUPixFmt outFmt = { /* sane defaults? */
        .fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
        .compMap = 0x00010203,
    };
    switch (format) {
    /*  packed16 formats: 4 bits/channel */
        case SDL_PIXELFORMAT_RGB444: /* aka XRGB4444 */
        case SDL_PIXELFORMAT_ARGB4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x01020300;
            break;
        }
        case SDL_PIXELFORMAT_RGBA4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_ABGR4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x03020100;
            break;
        }
        case SDL_PIXELFORMAT_BGRA4444: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
            outFmt.compMap = 0x02010003;
            break;
        }

    /*  packed16 formats: 5 bits/channel */
        case SDL_PIXELFORMAT_RGB555: /* aka XRGB1555 */
        case SDL_PIXELFORMAT_ARGB1555: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x01020300;
            break;
        }
        case SDL_PIXELFORMAT_BGR555: /* aka XRGB1555 */
        case SDL_PIXELFORMAT_ABGR1555: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x03020100;
            break;
        }
        case SDL_PIXELFORMAT_RGBA5551: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_BGRA5551: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
            outFmt.compMap = 0x02010003;
            break;
        }

    /*  packed16 formats: 565 */
        case SDL_PIXELFORMAT_RGB565: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_BGR565: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
            outFmt.compMap = 0x02010003;
            break;
        }

    /*  packed32 formats */
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_RGBX8888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x00010203;
            break;
        }
        case SDL_PIXELFORMAT_ARGB8888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x01020300;
            break;
        }
        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_BGRX8888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x02010003;
            break;
        }
        case SDL_PIXELFORMAT_ABGR8888:
        case SDL_PIXELFORMAT_BGR888: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            outFmt.compMap = 0x03020100;
            break;
        }
        case SDL_PIXELFORMAT_ARGB2101010: {
            outFmt.fmt = GX2_SURFACE_FORMAT_UNORM_R10_G10_B10_A2;
            outFmt.compMap = 0x01020300;
            break;
        }
        default: {
        /*  TODO return an error */
            printf("SDL: WiiU format not recognised (SDL: %08X)\n", format);
            break;
        }
    }

    return outFmt;
}

#endif //SDL_render_wiiu_h
