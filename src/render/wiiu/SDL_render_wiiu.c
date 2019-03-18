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

#if SDL_VIDEO_RENDER_WIIU

#include "../../video/wiiu/SDL_wiiuvideo.h"
#include "../../video/wiiu/wiiu_shaders.h"
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include "SDL_render_wiiu.h"

#include <gx2/event.h>
#include <gx2/registers.h>
#include <gx2r/surface.h>

#include <malloc.h>

SDL_RenderDriver WIIU_RenderDriver;

SDL_Renderer *WIIU_SDL_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_Renderer *renderer;
    WIIU_RenderData *data;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (WIIU_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        WIIU_SDL_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    /* Setup renderer functions */
    renderer->WindowEvent = WIIU_SDL_WindowEvent;
    renderer->GetOutputSize = WIIU_SDL_GetOutputSize;
    renderer->CreateTexture = WIIU_SDL_CreateTexture;
    renderer->SetTextureColorMod = WIIU_SDL_SetTextureColorMod;
    renderer->SetTextureAlphaMod = WIIU_SDL_SetTextureAlphaMod;
    renderer->UpdateTexture = WIIU_SDL_UpdateTexture;
    renderer->LockTexture = WIIU_SDL_LockTexture;
    renderer->UnlockTexture = WIIU_SDL_UnlockTexture;
    renderer->SetRenderTarget = WIIU_SDL_SetRenderTarget;
    renderer->UpdateViewport = WIIU_SDL_UpdateViewport;
    renderer->UpdateClipRect = WIIU_SDL_UpdateClipRect;
    renderer->RenderClear = WIIU_SDL_RenderClear;
    renderer->RenderDrawPoints = WIIU_SDL_RenderDrawPoints;
    renderer->RenderDrawLines = WIIU_SDL_RenderDrawLines;
    renderer->RenderFillRects = WIIU_SDL_RenderFillRects;
    renderer->RenderCopy = WIIU_SDL_RenderCopy;
    renderer->RenderCopyEx = WIIU_SDL_RenderCopyEx;
    renderer->RenderReadPixels = WIIU_SDL_RenderReadPixels;
    renderer->RenderPresent = WIIU_SDL_RenderPresent;
    renderer->DestroyTexture = WIIU_SDL_DestroyTexture;
    renderer->DestroyRenderer = WIIU_SDL_DestroyRenderer;
    renderer->info = WIIU_RenderDriver.info;
    renderer->driverdata = data;
    renderer->window = window;

    /* Prepare shaders */
    wiiuInitTextureShader();
    wiiuInitColorShader();

    /* List of attibutes to free after render */
    data->listfree = NULL;

    /* Setup line and point size */
    GX2SetLineWidth(1.0f);
    GX2SetPointSize(1.0f, 1.0f);

    /* Create a fresh context state */
    data->ctx = (GX2ContextState *) memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));
    SDL_memset(data->ctx, 0, sizeof(GX2ContextState));
    GX2SetupContextStateEx(data->ctx, TRUE);
    GX2SetContextState(data->ctx);

    /* Setup some context state options */
    GX2SetAlphaTest(TRUE, GX2_COMPARE_FUNC_GREATER, 0.0f);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_NEVER);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);

    /* Make a texture for the window */
    WIIU_SDL_CreateWindowTex(renderer, window);

    /* Setup colour buffer, rendering to the window */
    WIIU_SDL_SetRenderTarget(renderer, NULL);

    return renderer;
}

void WIIU_SDL_CreateWindowTex(SDL_Renderer * renderer, SDL_Window * window)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    const char *s_hint;
    SDL_ScaleMode s_mode;

    if (data->windowTex.driverdata) {
        WIIU_SDL_DestroyTexture(renderer, &data->windowTex);
        data->windowTex = (SDL_Texture) {0};
    }

    /* Setup scaling mode; this is normally handled by
       SDL_CreateTexture/SDL_GetScaleMode, but those can't
       be called before fully initializinig the renderer */
    s_hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    if (!s_hint || SDL_strcasecmp(s_hint, "nearest") == 0) {
        s_mode = SDL_ScaleModeNearest;
    } else if (SDL_strcasecmp(s_hint, "linear") == 0) {
        s_mode = SDL_ScaleModeLinear;
    } else if (SDL_strcasecmp(s_hint, "best") == 0) {
        s_mode = SDL_ScaleModeBest;
    } else {
        s_mode = (SDL_ScaleMode)SDL_atoi(s_hint);
    }

    /* Allocate a buffer for the window */
    data->windowTex = (SDL_Texture) {
        .format = SDL_PIXELFORMAT_RGBA8888,
        .r = 255, .g = 255, .b = 255, .a = 255,
        .driverdata = WIIU_TEXTURE_MEM1_MAGIC,
        .scaleMode = s_mode,
    };

    SDL_GetWindowSize(window, &data->windowTex.w, &data->windowTex.h);

    /* Setup texture and color buffer for the window */
    WIIU_SDL_CreateTexture(renderer, &data->windowTex);
}

int WIIU_SDL_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    /* Set window or texture as target */
    WIIU_TextureData *tdata = (WIIU_TextureData *)((texture) ? texture->driverdata
                                                             : data->windowTex.driverdata);

    /* Wait for the texture rendering to finish */
    WIIU_TextureCheckWaitRendering(data, tdata);

    /* Update u_viewSize */
    data->u_viewSize = (WIIUVec4) {
        .x = (float)tdata->cbuf.surface.width,
        .y = (float)tdata->cbuf.surface.height,
    };

    /* Update context state */
    GX2SetColorBuffer(&tdata->cbuf, GX2_RENDER_TARGET_0);

    /* These may be unnecessary - see SDL_render.c: SDL_SetRenderTarget's calls
       to UpdateViewport and UpdateClipRect. TODO for once the render is
       basically working */
    GX2SetViewport(0, 0, (float)tdata->cbuf.surface.width, (float)tdata->cbuf.surface.height, 0.0f, 1.0f);
    GX2SetScissor(0, 0, (float)tdata->cbuf.surface.width, (float)tdata->cbuf.surface.height);

    return 0;
}

void WIIU_SDL_DestroyRenderer(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    GX2DrawDone();

    WIIU_FreeRenderData(data);
    WIIU_TextureDoneRendering(data);

    free(data->ctx);

    wiiuFreeColorShader();
    wiiuFreeTextureShader();

    SDL_free(data);
    SDL_free(renderer);
}

int WIIU_SDL_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                              Uint32 format, void * pixels, int pitch)
{
    SDL_Texture* target = WIIU_GetRenderTarget(renderer);
    WIIU_TextureData* tdata = (WIIU_TextureData*) target->driverdata;
    Uint8 *src_image;
    int ret;

    /* NOTE: The rect is already adjusted according to the viewport by
       SDL_RenderReadPixels */

    if (rect->x < 0 || rect->x+rect->w > tdata->cbuf.surface.width ||
        rect->y < 0 || rect->y+rect->h > tdata->cbuf.surface.height) {
        return SDL_SetError("Tried to read outside of surface bounds");
    }

    src_image = GX2RLockSurfaceEx(&tdata->cbuf.surface, 0, GX2R_RESOURCE_LOCKED_READ_ONLY);

    /* Convert and copy the pixels to target buffer */
    ret = SDL_ConvertPixels(rect->w, rect->h, target->format,
                            src_image + rect->y * tdata->cbuf.surface.pitch + rect->x * 4,
                            tdata->cbuf.surface.pitch,
                            format, pixels, pitch);

    GX2RUnlockSurfaceEx(&tdata->cbuf.surface, 0, GX2R_RESOURCE_LOCKED_READ_ONLY);

    return ret;
}


SDL_RenderDriver WIIU_RenderDriver =
{
    .CreateRenderer = WIIU_SDL_CreateRenderer,
    .info = {
        .name = "WiiU GX2",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 13, //21,
        .texture_formats = {
        /*  TODO: Alpha-less (X) formats */
            SDL_PIXELFORMAT_RGBA8888,
//            SDL_PIXELFORMAT_RGBX8888,

//            SDL_PIXELFORMAT_RGB444,
            SDL_PIXELFORMAT_ARGB4444,
            SDL_PIXELFORMAT_RGBA4444,
            SDL_PIXELFORMAT_ABGR4444,
            SDL_PIXELFORMAT_BGRA4444,

//            SDL_PIXELFORMAT_RGB555,
            SDL_PIXELFORMAT_ARGB1555,
//            SDL_PIXELFORMAT_BGR555,
            SDL_PIXELFORMAT_ABGR1555,
            SDL_PIXELFORMAT_RGBA5551,
            SDL_PIXELFORMAT_BGRA5551,

        /*  TODO: RGB565 doesn't seem to work right, endian issue? */
//            SDL_PIXELFORMAT_RGB565,
//            SDL_PIXELFORMAT_BGR565,

            SDL_PIXELFORMAT_ARGB8888,
            SDL_PIXELFORMAT_BGRA8888,
//            SDL_PIXELFORMAT_BGRX8888,
            SDL_PIXELFORMAT_ABGR8888,
//            SDL_PIXELFORMAT_BGR888,

            SDL_PIXELFORMAT_ARGB2101010,
        },
        .max_texture_width = 0,
        .max_texture_height = 0,
    },
};

#endif /* SDL_VIDEO_RENDER_WIIU */

/* vi: set ts=4 sw=4 expandtab: */
