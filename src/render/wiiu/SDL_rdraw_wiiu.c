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
#include "../../video/wiiu/wiiu_shaders.h"
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include "SDL_render_wiiu.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int WIIU_SDL_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                        const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2Texture *wiiu_tex = (GX2Texture*) texture->driverdata;

    float swidth = data->cbuf.surface.width;
    float sheight = data->cbuf.surface.height;

    //TODO: Move texCoord/pos math to shader / GX2SetViewport
    float tr_x = (((renderer->viewport.x + dstrect->x) / swidth)*2.0f)-1.0f;
    float tr_y = (((renderer->viewport.y + dstrect->y) / sheight)*2.0f)-1.0f;
    float tr_w = (dstrect->w / swidth) * 2.0f;
    float tr_h = (dstrect->h / sheight) * 2.0f;

    float vb[] =
    {
        tr_x, tr_y,
        tr_x + tr_w, tr_y,
        tr_x + tr_w, tr_y + tr_h,
        tr_x, tr_y + tr_h,
    };

    GX2RLockBufferEx(&data->texPositionBuffer, 0);
    memcpy(data->texPositionBuffer.buffer, vb, data->texPositionBuffer.elemCount * data->texPositionBuffer.elemSize);
    GX2RUnlockBufferEx(&data->texPositionBuffer, 0);

    float tex_x_min = (float)srcrect->x / swidth;
    float tex_x_max = (float)(srcrect->x + srcrect->w) / swidth;
    float tex_y_min = (float)srcrect->y / sheight;
    float tex_y_max = (float)(srcrect->y + srcrect->h) / sheight;

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
    wiiuSetTextureShader();

    GX2SetPixelTexture(texture, 0);
    GX2SetPixelSampler(sampler, 0);

    GX2RSetAttributeBuffer(&data->texPositionBuffer, 0, data->texPositionBuffer.elemSize, 0);
    GX2RSetAttributeBuffer(&data->texCoordBuffer, 1, data->texCoordBuffer.elemSize, 0);
    GX2RSetAttributeBuffer(&data->texColourBuffer, 2, data->texColourBuffer.elemSize, 0);

    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

int WIIU_SDL_RenderDrawPoints(SDL_Renderer * renderer, const SDL_FPoint * points,
                              int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_RenderAllocData *rdata = SDL_malloc(sizeof(WIIU_RenderAllocData) + sizeof(float) * 2 * count);
    if (!rdata) return SDL_OutOfMemory();
    float *vb = rdata->ptr;

    /* Compute vertex pos */
    float swidth = data->cbuf.surface.width;
    float sheight = data->cbuf.surface.height;
    float vx = (float)renderer->viewport.x;
    float vy = (float)renderer->viewport.y;

    for (int i = 0; i < count; ++i) {
        vb[i*2+0] = (((vx + points[i].x) / swidth) * 2.0f)-1.0f;
        vb[i*2+1] = (((vy + points[i].y) / sheight) * 2.0f)-1.0f;
    }

    /* Render points */
    float u_color[4] = {(float)renderer->r / 255.0f,
                        (float)renderer->g / 255.0f,
                        (float)renderer->b / 255.0f,
                        (float)renderer->a / 255.0f};

    GX2SetContextState(&data->ctx);
    wiiuSetColorShader();

    GX2SetAttribBuffer(0, sizeof(float) * 2 * count, sizeof(float) * 2, vb);
    GX2SetPixelUniformReg(wiiuColorShader->pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_color);

    GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, count, 0, 1);

    /* vertex buffer can't be free'd until rendering is finished */
    rdata->next = data->listfree;
    data->listfree = rdata;

    return 0;
}


int WIIU_SDL_RenderDrawLines(SDL_Renderer * renderer, const SDL_FPoint * points,
                             int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_RenderAllocData *rdata = SDL_malloc(sizeof(WIIU_RenderAllocData) + sizeof(float) * 2 * count);
    if (!rdata) return SDL_OutOfMemory();
    float *vb = rdata->ptr;

    /* Compute vertex pos */
    float swidth = data->cbuf.surface.width;
    float sheight = data->cbuf.surface.height;
    float vx = (float)renderer->viewport.x;
    float vy = (float)renderer->viewport.y;

    for (int i = 0; i < count; ++i) {
        vb[i*2+0] = (((vx + points[i].x) / swidth) * 2.0f) - 1.0f;
        vb[i*2+1] = (((vy + points[i].y) / sheight) * 2.0f) - 1.0f;
    }

    /* Render lines */
    float u_color[4] = {(float)renderer->r / 255.0f,
                        (float)renderer->g / 255.0f,
                        (float)renderer->b / 255.0f,
                        (float)renderer->a / 255.0f};

    GX2SetContextState(&data->ctx);
    wiiuSetColorShader();

    GX2SetAttribBuffer(0, sizeof(float) * 2 * count, sizeof(float) * 2, vb);
    GX2SetPixelUniformReg(wiiuColorShader->pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_color);

    GX2DrawEx(GX2_PRIMITIVE_MODE_LINE_STRIP, count, 0, 1);

    /* vertex buffer can't be free'd until rendering is finished */
    rdata->next = data->listfree;
    data->listfree = rdata;

    return 0;
}

int WIIU_SDL_RenderFillRects(SDL_Renderer * renderer, const SDL_FRect * rects, int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_RenderAllocData *rdata = SDL_malloc(sizeof(WIIU_RenderAllocData) + sizeof(float) * 2 * 4 * count);
    if (!rdata) return SDL_OutOfMemory();
    float *vb = rdata->ptr;

    /* Compute vertex pos */
    float swidth = data->cbuf.surface.width;
    float sheight = data->cbuf.surface.height;
    float vx = (float)renderer->viewport.x;
    float vy = (float)renderer->viewport.y;

    for (int i = 0; i < count; ++i) {
        float tr_x = (((vx + rects[i].x) / swidth) * 2.0f) - 1.0f;
        float tr_y = (((vy + rects[i].y) / sheight) * 2.0f) - 1.0f;
        float tr_w = (rects[i].w / swidth) * 2.0f;
        float tr_h = (rects[i].h / sheight) * 2.0f;
        vb[i*8+0] = tr_x;
        vb[i*8+1] = tr_y;
        vb[i*8+2] = tr_x + tr_w;
        vb[i*8+3] = tr_y;
        vb[i*8+4] = tr_x + tr_w;
        vb[i*8+5] = tr_y + tr_h;
        vb[i*8+6] = tr_x;
        vb[i*8+7] = tr_y + tr_h;
    }

    /* Render rects */
    float u_color[4] = {(float)renderer->r / 255.0f,
                        (float)renderer->g / 255.0f,
                        (float)renderer->b / 255.0f,
                        (float)renderer->a / 255.0f};

    GX2SetContextState(&data->ctx);
    wiiuSetColorShader();

    GX2SetAttribBuffer(0, sizeof(float) * 2 * 4 * count, sizeof(float) * 2, vb);
    GX2SetPixelUniformReg(wiiuColorShader->pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_color);

    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4 * count, 0, 1);

    /* vertex buffer can't be free'd until rendering is finished */
    rdata->next = data->listfree;
    data->listfree = rdata;

    return 0;
}

void WIIU_SDL_RenderPresent(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    SDL_Window *window = renderer->window;

    GX2Flush();
    GX2DrawDone();
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, data->cbuf.surface.image, data->cbuf.surface.imageSize);

    if (window) {
        SDL_UpdateWindowSurface(window);
    }

    while (data->listfree) {
        void *ptr = data->listfree;
        data->listfree = data->listfree->next;
        SDL_free(ptr);        
    }
}

int WIIU_SDL_RenderClear(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2ClearColor(&data->cbuf,
                  (float)renderer->r / 255.0f,
                  (float)renderer->g / 255.0f,
                  (float)renderer->b / 255.0f,
                  (float)renderer->a / 255.0f);
    return 0;
}

#endif //SDL_VIDEO_RENDER_WIIU
