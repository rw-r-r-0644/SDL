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

SDL_Renderer *
WIIU_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_Surface *surface
    SDL_Renderer *renderer;
    WIIU_RenderData *data;

    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        SDL_SetError("Can't create renderer for NULL surface");
        return NULL;
    }

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

    renderer->WindowEvent = WIIU_WindowEvent;
    renderer->GetOutputSize = WIIU_GetOutputSize;
    renderer->CreateTexture = WIIU_CreateTexture;
    renderer->SetTextureColorMod = WIIU_SetTextureColorMod;
    renderer->SetTextureAlphaMod = WIIU_SetTextureAlphaMod;
    renderer->SetTextureBlendMode = WIIU_SetTextureBlendMode;
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

    // Setup texture shader attribute buffers
    data->texPositionBuffer.flags =
    data->texCoordBuffer.flags =
    data->texColourBuffer.flags =
        GX2R_RESOURCE_BIND_VERTEX_BUFFER |
        GX2R_RESOURCE_USAGE_CPU_READ |
        GX2R_RESOURCE_USAGE_CPU_WRITE |
        GX2R_RESOURCE_USAGE_GPU_READ;
    data->texPositionBuffer.elemSize = sizeof(float) * 3;
    data->texCoordBuffer.elemSize = sizeof(float) * 2;
    data->texColourBuffer.elemSize = sizeof(float) * 4;
    data->texPositionBuffer.elemCount =
    data->texCoordBuffer.elemCount =
    data->texColourBuffer.elemCount = 4;

    GX2RCreateBuffer(&data->texPositionBuffer);
    GX2RCreateBuffer(&data->texCoordBuffer);
    GX2RCreateBuffer(&data->texColourBuffer);

    // Prepare texture color buffer data (it's costant between textures)
    float aColourDefault[] = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
    };
    GX2RLockBufferEx(&data->texColourBuffer, 0);
    memcpy(data->texColourBuffer.buffer, aColourDefault, data->texColourBuffer.elemCount * data->texColourBuffer.elemSize);
    GX2RUnlockBufferEx(&data->texColourBuffer, 0);

    // Setup sampler
    GX2InitSampler(&data->sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    // Create a fresh context state
    GX2SetupContextStateEx(&data->ctx, TRUE);

    // Setup colour buffer, rendering to the window
    WIIU_SetRenderTarget(renderer, NULL);

    return renderer;
}

static int
WIIU_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
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
    GX2SetViewport(0, 0, (float)data->cbuf.surface.width, (float)data->cbuf.surface.height, 0.0f, 1.0f);
    GX2SetScissor(0, 0, (float)data->cbuf.surface.width, (float)data->cbuf.surface.height);

    return 0;
}

static int
WIIU_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2Texture *wiiu_tex = (GX2Texture*) texture->driverdata;

    //TODO: Move texCoord/pos math to shader
    float transform_x, transform_y;
    if (renderer->viewport.x || renderer->viewport.y) {
        transform_x = (((renderer->viewport.x + dstrect->x) / (float)data->cbuf.surface.width) * 2.0f)-1.0f;
        transform_y = (((renderer->viewport.y + dstrect->y) / (float)data->cbuf.surface.height) * 2.0f)-1.0f;
    } else {
        transform_x = ((dstrect->x / (float)data->cbuf.surface.width) * 2.0f)-1.0f;
        transform_y = ((dstrect->y / (float)data->cbuf.surface.height) * 2.0f)-1.0f;
    }
    float transform_width = (dstrect->w / (float)data->cbuf.surface.width) * 2.0f;
    float transform_height = (dstrect->h / (float)data->cbuf.surface.height) * 2.0f;

    float aPosition[] =
    {
        transform_x, transform_y, 0.0f,
        transform_x + transform_width, transform_y, 0.0f,
        transform_x + transform_width, transform_y + transform_height, 0.0f,
        transform_x, transform_y + transform_height, 0.0f,
    };

    GX2RLockBufferEx(&data->texPositionBuffer, 0);
    memcpy(data->texPositionBuffer.buffer, aPosition, data->texPositionBuffer.elemCount * data->texPositionBuffer.elemSize);
    GX2RUnlockBufferEx(&data->texPositionBuffer, 0);

    float tex_x_min = (float)srcrect->x / (float)wiiu_tex->surface.width;
    float tex_x_max = (float)(srcrect->x + srcrect->w) / (float)wiiu_tex->surface.width;
    float tex_y_min = (float)srcrect->y / (float)wiiu_tex->surface.height;
    float tex_y_max = (float)(srcrect->y + srcrect->h) / (float)wiiu_tex->surface.height;

    float aTexCoord[] =
    {
        tex_x_min, tex_y_max,
        tex_x_max, tex_y_max,
        tex_x_max, tex_y_min,
        tex_x_min, tex_y_min,
    };

    GX2RLockBufferEx(&data->texCoordBuffer, 0);
    memcpy(data->texCoordBuffer.buffer, aTexCoord, data->texCoordBuffer.elemCount * data->texCoordBuffer.elemSize);
    GX2RUnlockBufferEx(&data->texCoordBuffer, 0);

    //RENDER
    GX2SetContextState(&data->ctx);

    GX2SetFetchShader(&texShaders.fetchShader);
    GX2SetVertexShader(texShaders.vertexShader);
    GX2SetPixelShader(texShaders.pixelShader);

    GX2SetPixelTexture(texture, 0);
    GX2SetPixelSampler(sampler, 0);

    GX2RSetAttributeBuffer(&data->texPositionBuffer, 0, data->texPositionBuffer.elemSize, 0);
    GX2RSetAttributeBuffer(&data->texCoordBuffer, 1, data->texCoordBuffer.elemSize, 0);
    GX2RSetAttributeBuffer(&data->texColourBuffer, 2, data->texColourBuffer.elemSize, 0);

    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

static void
WIIU_RenderPresent(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    SDL_Window *window = renderer->window;

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE, data->cbuf.surface.image, data->cbuf.surface.imageSize);

    if (window) {
        SDL_UpdateWindowSurface(window);
    }
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
