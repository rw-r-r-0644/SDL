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
WIIU_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2Texture *wiiu_tex = (GX2Texture*) texture->driverdata;

    //TODO: Move texCoord/pos math to shader / GX2SetViewport
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

static int
WIIU_RenderClear(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2ClearColor(&data->cbuf, renderer->r, renderer->g, renderer->b, renderer->a);
    return 0;
}

#endif //SDL_VIDEO_RENDER_WIIU
