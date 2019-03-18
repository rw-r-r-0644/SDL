/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2019 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2019 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>

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
#include <gx2r/buffer.h>
#include <gx2/context.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include <gx2/surface.h>
#include <gx2/event.h>

/* Driver internal data structures */
typedef struct WIIUVec2 WIIUVec2;
typedef struct WIIUVec3 WIIUVec3;
typedef struct WIIUVec4 WIIUVec4;
typedef struct WIIUPixFmt WIIUPixFmt;
typedef struct WIIU_RenderAllocData WIIU_RenderAllocData;
typedef struct WIIU_TextureDrawData WIIU_TextureDrawData;
typedef struct WIIU_RenderData WIIU_RenderData;
typedef struct WIIU_TextureData WIIU_TextureData;

struct WIIUVec2
{
    union { float x, r; };
    union { float y, g; };
};

struct WIIUVec3
{
    union { float x, r; };
    union { float y, g; };
    union { float z, b; };
};

struct WIIUVec4
{
    union { float x, r; };
    union { float y, g; };
    union { float z, b; };
    union { float w, a; };
};

struct WIIUPixFmt
{
    GX2SurfaceFormat fmt;
    uint32_t compMap;
};

struct WIIU_RenderAllocData
{
    void *next;
    GX2RBuffer buffer;
};

struct WIIU_TextureDrawData
{
    void *next;
    WIIU_TextureData *texdata;
};

struct WIIU_RenderData
{
    GX2ContextState *ctx;
    WIIU_RenderAllocData *listfree;
    WIIU_TextureDrawData *listdraw;
    WIIUVec4 u_viewSize;
    SDL_Texture windowTex;
};

struct WIIU_TextureData
{
    GX2Sampler sampler;
    GX2Texture texture;
    GX2ColorBuffer cbuf;
    WIIUVec4 u_texSize;
    WIIUVec4 u_mod;
    int isRendering;
};

/* Ask texture driver to allocate texture's memory from MEM1 */
#define WIIU_TEXTURE_MEM1_MAGIC (void *)0xCAFE0001

/* SDL_render API implementation */
SDL_Renderer *WIIU_SDL_CreateRenderer(SDL_Window * window, Uint32 flags);
void WIIU_SDL_WindowEvent(SDL_Renderer * renderer,
                             const SDL_WindowEvent *event);
int WIIU_SDL_GetOutputSize(SDL_Renderer * renderer, int *w, int *h);
int WIIU_SDL_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
int WIIU_SDL_SetTextureColorMod(SDL_Renderer * renderer,
                                SDL_Texture * texture);
int WIIU_SDL_SetTextureAlphaMod(SDL_Renderer * renderer,
                                SDL_Texture * texture);
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

/* Driver internal functions */
void WIIU_SDL_CreateWindowTex(SDL_Renderer * renderer, SDL_Window * window);

/* Utility/helper functions */
static inline GX2RBuffer * WIIU_AllocRenderData(WIIU_RenderData *r, GX2RBuffer buffer)
{
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

static inline void WIIU_FreeRenderData(WIIU_RenderData *r)
{
    while (r->listfree) {
        WIIU_RenderAllocData *ptr = r->listfree;
        r->listfree = r->listfree->next;
        GX2RDestroyBufferEx(&ptr->buffer, 0);
        SDL_free(ptr);
    }
}

static inline void WIIU_TextureStartRendering(WIIU_RenderData *r, WIIU_TextureData *t)
{
    WIIU_TextureDrawData *d = SDL_malloc(sizeof(WIIU_TextureDrawData));
    t->isRendering = 1;
    d->texdata = t;
    d->next = r->listdraw;
    r->listdraw = d;
}

static inline void WIIU_TextureDoneRendering(WIIU_RenderData *r)
{
    while (r->listdraw) {
        WIIU_TextureDrawData *d = r->listdraw;
        r->listdraw = r->listdraw->next;
        d->texdata->isRendering = 0;
        SDL_free(d);
    }
}

/* If the texture is currently being rendered and we change the content
   before the rendering is finished, the GPU will end up partially drawing
   the new data, so we wait for the GPU to finish rendering before
   updating the texture */
static inline void WIIU_TextureCheckWaitRendering(WIIU_RenderData *r, WIIU_TextureData *t)
{
    if (t->isRendering) {
        GX2DrawDone();
        WIIU_TextureDoneRendering(r);
        WIIU_FreeRenderData(r);
    }
}

static inline SDL_Texture * WIIU_GetRenderTarget(SDL_Renderer* renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    if (renderer->target) {
        return renderer->target;
    }

    return &data->windowTex;
}

static inline WIIUPixFmt SDLFormatToWIIUFormat(Uint32 format)
{
    WIIUPixFmt outFmt = { /* sane defaults? */
        .fmt = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
        .compMap = 0x00010203,
    };

    switch (format) {
        /* packed16 formats: 4 bits/channel */
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

        /* packed16 formats: 5 bits/channel */
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

        /* packed16 formats: 565 */
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

        /* packed32 formats */
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
            /* TODO return an error */
            printf("SDL: WiiU format not recognised (SDL: %08X)\n", format);
            break;
        }
    }

    return outFmt;
}

#endif //SDL_render_wiiu_h
