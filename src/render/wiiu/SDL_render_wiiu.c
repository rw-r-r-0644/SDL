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

/* SDL surface based renderer implementation */

SDL_Renderer *WIIU_SDL_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_Surface *surface
    SDL_Renderer *renderer;
    WIIU_RenderData *data;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (WIIU_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        WIIU_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    // See sdl_render_wiiu.h for explanations of commented-out functions

    renderer->WindowEvent = WIIU_WindowEvent;
    renderer->GetOutputSize = WIIU_GetOutputSize;
    renderer->CreateTexture = WIIU_CreateTexture;
    //renderer->SetTextureColorMod = WIIU_SetTextureColorMod;
    //renderer->SetTextureAlphaMod = WIIU_SetTextureAlphaMod;
    //renderer->SetTextureBlendMode = WIIU_SetTextureBlendMode;
    renderer->UpdateTexture = WIIU_UpdateTexture;
    renderer->LockTexture = WIIU_LockTexture;
    renderer->UnlockTexture = WIIU_UnlockTexture;
    renderer->SetRenderTarget = WIIU_SetRenderTarget;
    renderer->UpdateViewport = WIIU_UpdateViewport;
    renderer->UpdateClipRect = WIIU_UpdateClipRect;
    renderer->RenderClear = WIIU_RenderClear;
    renderer->RenderDrawPoints = WIIU_RenderDrawPoints;
    renderer->RenderDrawLines = WIIU_RenderDrawLines;
    renderer->RenderFillRects = WIIU_RenderFillRects;
    renderer->RenderCopy = WIIU_RenderCopy;
    renderer->RenderCopyEx = WIIU_RenderCopyEx;
    renderer->RenderReadPixels = WIIU_RenderReadPixels;
    renderer->RenderPresent = WIIU_RenderPresent;
    renderer->DestroyTexture = WIIU_DestroyTexture;
    renderer->DestroyRenderer = WIIU_DestroyRenderer;
    renderer->info = WIIU_RenderDriver.info;
    renderer->driverdata = data;
    renderer->window = window;

    // Prepare shaders
    wiiuInitTextureShader();
    wiiuInitColorShader();

    // List of attibutes to free after render
    data->listfree = NULL;

    // Setup texture shader attribute buffers
    data->texPositionBuffer.flags =
    data->texCoordBuffer.flags =
        GX2R_RESOURCE_BIND_VERTEX_BUFFER |
        GX2R_RESOURCE_USAGE_CPU_READ |
        GX2R_RESOURCE_USAGE_CPU_WRITE |
        GX2R_RESOURCE_USAGE_GPU_READ;
    data->texPositionBuffer.elemSize = sizeof(float) * 2;
    data->texCoordBuffer.elemSize = sizeof(float) * 2;
    data->texPositionBuffer.elemCount =
    data->texCoordBuffer.elemCount = 4;

    GX2RCreateBuffer(&data->texPositionBuffer);
    GX2RCreateBuffer(&data->texCoordBuffer);

    // Setup line and point size
    GX2SetLineWidth(1.0f);
    GX2SetPointSize(1.0f, 1.0f);

    // Setup sampler
    GX2InitSampler(&data->sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    // Create a fresh context state
    GX2SetupContextStateEx(&data->ctx, TRUE);

    // Setup colour buffer, rendering to the window
    WIIU_SetRenderTarget(renderer, NULL);

    return renderer;
}

int WIIU_SDL_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    GX2Surface *target;

    if (texture) {
        // Set texture as target
        GX2Texture *wiiu_tex = (GX2Texture *) texture->driverdata;
        target = wiiu_tex->surface;
    } else {
        // Set window texture as target
        WIIU_WindowData *wdata = (WIIU_WindowData *) SDL_GetWindowData(window, WIIU_WINDOW_DATA);
        target = &wdata->texture.surface;
    }

    // Update color buffer
    memset(&data->cbuf, 0, sizeof(data->cbuf));
    memcpy(&data->cbuf.surface, target, sizeof(GX2Surface));
    data->cbuf.surface.use = GX2_SURFACE_USE_TEXTURE | GX2_SURFACE_USE_COLOR_BUFFER;
    data->cbuf.viewNumSlices = 1;
    GX2InitColorBufferRegs(&data->cbuf);

    // Update context state
    GX2SetContextState(&data->ctx);
    GX2SetColorBuffer(&data->cbuf, GX2_RENDER_TARGET_0);
    // These may be unnecessary - see SDL_render.c: SDL_SetRenderTarget's calls
    // to UpdateViewport and UpdateClipRect. TODO for once the render is
    // basically working.
    GX2SetViewport(0, 0, (float)data->cbuf.surface.width, (float)data->cbuf.surface.height, 0.0f, 1.0f);
    GX2SetScissor(0, 0, (float)data->cbuf.surface.width, (float)data->cbuf.surface.height);

    return 0;
}

void WIIU_SDL_DestroyRenderer(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    while (data->listfree) {
        void *ptr = data->listfree;
        data->listfree = data->listfree->next;
        SDL_free(ptr);        
    }

    wiiuFreeColorShader();
    wiiuFreeTextureShader();

    SDL_free(data);
    SDL_free(renderer);
}

int WIIU_SDL_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                              Uint32 format, void * pixels, int pitch)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    Uint32 src_format;
    void *src_pixels;

    /* NOTE: The rect is already adjusted according to the viewport by
     * SDL_RenderReadPixels.
     */

    if (rect->x < 0 || rect->x+rect->w > data->cbuf.surface.width ||
        rect->y < 0 || rect->y+rect->h > data->cbuf.surface.height) {
        return SDL_SetError("Tried to read outside of surface bounds");
    }

    src_format = SDL_PIXELFORMAT_RGBA8888; // TODO once working: other formats/checks
    src_pixels = (void*)((Uint8 *) surface->pixels +
                    rect->y * data->cbuf.surface.pitch +
                    rect->x * 4);

    return SDL_ConvertPixels(rect->w, rect->h,
                             src_format, src_pixels, data->cbuf.surface.pitch,
                             format, pixels, pitch);
}


SDL_RenderDriver WIIU_RenderDriver = {
    .CreateRenderer = WIIU_CreateRenderer,
    .info = {
        .name = "WiiU GX2",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 5,
        .texture_formats = {
            SDL_PIXELFORMAT_RGBA8888,
            SDL_PIXELFORMAT_RGBA4444,
            SDL_PIXELFORMAT_ABGR1555,
            SDL_PIXELFORMAT_RGBA5551,
            SDL_PIXELFORMAT_RGB565,
        },
        .max_texture_width = 0,
        .max_texture_height = 0,
    },
};

#endif /* SDL_VIDEO_RENDER_WIIU */

/* vi: set ts=4 sw=4 expandtab: */
