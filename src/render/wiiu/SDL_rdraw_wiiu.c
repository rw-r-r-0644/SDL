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

#include <gx2/texture.h>
#include <gx2/draw.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/state.h>
#include <gx2/clear.h>
#include <gx2/mem.h>
#include <gx2/event.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int WIIU_SDL_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                        const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;

    /* Compute vertex points */
    float x_min = renderer->viewport.x + dstrect->x;
    float y_min = renderer->viewport.y + dstrect->y;
    float x_max = renderer->viewport.x + dstrect->x + dstrect->w;
    float y_max = renderer->viewport.y + dstrect->y + dstrect->h;
    float *a_position = WIIU_AllocRenderData(data, sizeof(float) * 8);
    a_position[0] = x_min;  a_position[1] = y_min;
    a_position[2] = x_max;  a_position[3] = y_min;
    a_position[4] = x_max;  a_position[5] = y_max;
    a_position[6] = x_min;  a_position[7] = y_max;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, a_position, sizeof(float) * 8);

    /* Compute texture coords */
    float *a_texCoord = WIIU_AllocRenderData(data, sizeof(float) * 8);
    a_texCoord[0] = srcrect->x;                a_texCoord[1] = srcrect->y + srcrect->h;
    a_texCoord[2] = srcrect->x + srcrect->w;   a_texCoord[3] = srcrect->y + srcrect->h;
    a_texCoord[4] = srcrect->x + srcrect->w;   a_texCoord[5] = srcrect->y;
    a_texCoord[6] = srcrect->x;                a_texCoord[7] = srcrect->y;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, a_texCoord, sizeof(float) * 8);

    /* Render */
    GX2SetContextState(data->ctx);
    wiiuSetTextureShader();
    GX2SetPixelTexture(&tdata->texture, 0);
    GX2SetPixelSampler(&tdata->sampler, 0);
    GX2SetAttribBuffer(0, sizeof(float) * 8, sizeof(float) * 2, a_position);
    GX2SetAttribBuffer(1, sizeof(float) * 8, sizeof(float) * 2, a_texCoord);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)data->u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)tdata->u_texSize);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

    return 0;
}


int WIIU_SDL_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                          const double angle, const SDL_FPoint * center, const SDL_RendererFlip flip)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;

    /* Compute real vertex points */
    float x_min = renderer->viewport.x + dstrect->x;
    float y_min = renderer->viewport.y + dstrect->y;
    float x_max = x_min + dstrect->w;
    float y_max = y_min + dstrect->h;
    float cx = x_min + center->x;
    float cy = y_min + center->y;
    double r = angle * (M_PI / 180.0);
    float rvb[8] = {
        (flip & SDL_FLIP_HORIZONTAL) ? x_max : x_min, (flip & SDL_FLIP_VERTICAL) ? y_max : y_min,
        (flip & SDL_FLIP_HORIZONTAL) ? x_min : x_max, (flip & SDL_FLIP_VERTICAL) ? y_max : y_min,
        (flip & SDL_FLIP_HORIZONTAL) ? x_min : x_max, (flip & SDL_FLIP_VERTICAL) ? y_min : y_max,
        (flip & SDL_FLIP_HORIZONTAL) ? x_max : x_min, (flip & SDL_FLIP_VERTICAL) ? y_min : y_max,
    };
    float *a_position = WIIU_AllocRenderData(data, sizeof(float) * 8);
    for (int i = 0; i < 8; i += 2) {
        a_position[i+0] = cx + (SDL_cos(r) * (rvb[i+0] - cx) + SDL_sin(r) * (rvb[i+1] - cy));
        a_position[i+1] = cy + (SDL_cos(r) * (rvb[i+1] - cy) - SDL_sin(r) * (rvb[i+0] - cx));
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, a_position, sizeof(float) * 8);

    /* Compute texture coords */
    float *a_texCoord = WIIU_AllocRenderData(data, sizeof(float) * 8);
    a_texCoord[0] = srcrect->x;                a_texCoord[1] = srcrect->y + srcrect->h;
    a_texCoord[2] = srcrect->x + srcrect->w;   a_texCoord[3] = srcrect->y + srcrect->h;
    a_texCoord[4] = srcrect->x + srcrect->w;   a_texCoord[5] = srcrect->y;
    a_texCoord[6] = srcrect->x;                a_texCoord[7] = srcrect->y;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, a_texCoord, sizeof(float) * 8);

    /* Render */
    GX2SetContextState(data->ctx);
    wiiuSetTextureShader();
    GX2SetPixelTexture(&tdata->texture, 0);
    GX2SetPixelSampler(&tdata->sampler, 0);
    GX2SetAttribBuffer(0, sizeof(float) * 8, sizeof(float) * 2, a_position);
    GX2SetAttribBuffer(1, sizeof(float) * 8, sizeof(float) * 2, a_texCoord);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)data->u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)tdata->u_texSize);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

    return 0;
}

int WIIU_SDL_RenderDrawPoints(SDL_Renderer * renderer, const SDL_FPoint * points,
                              int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    /* Compute vertex pos */
    float *a_position = WIIU_AllocRenderData(data, sizeof(float) * 2 * count);
    for (int i = 0; i < count; ++i) {
        a_position[i*2+0] = (float)renderer->viewport.x + points[i].x;
        a_position[i*2+1] = (float)renderer->viewport.y + points[i].y;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, a_position, sizeof(float) * 2 * count);

    /* Render points */
    float u_color[4] = {(float)renderer->r / 255.0f,
                        (float)renderer->g / 255.0f,
                        (float)renderer->b / 255.0f,
                        (float)renderer->a / 255.0f};

    GX2SetContextState(data->ctx);
    wiiuSetColorShader();
    GX2SetAttribBuffer(0, sizeof(float) * 2 * count, sizeof(float) * 2, a_position);
    GX2SetVertexUniformReg(wiiuColorShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)data->u_viewSize);
    GX2SetPixelUniformReg(wiiuColorShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_color);
    GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, count, 0, 1);

    return 0;
}


int WIIU_SDL_RenderDrawLines(SDL_Renderer * renderer, const SDL_FPoint * points,
                             int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    /* Compute vertex pos */
    float *a_position = WIIU_AllocRenderData(data, sizeof(float) * 2 * count);
    for (int i = 0; i < count; ++i) {
        a_position[i*2+0] = (float)renderer->viewport.x + points[i].x;
        a_position[i*2+1] = (float)renderer->viewport.y + points[i].y;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, a_position, sizeof(float) * 2 * count);

    /* Render lines */
    float u_color[4] = {(float)renderer->r / 255.0f,
                        (float)renderer->g / 255.0f,
                        (float)renderer->b / 255.0f,
                        (float)renderer->a / 255.0f};

    GX2SetContextState(data->ctx);
    wiiuSetColorShader();
    GX2SetAttribBuffer(0, sizeof(float) * 2 * count, sizeof(float) * 2, a_position);
    GX2SetVertexUniformReg(wiiuColorShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)data->u_viewSize);
    GX2SetPixelUniformReg(wiiuColorShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_color);
    GX2DrawEx(GX2_PRIMITIVE_MODE_LINE_STRIP, count, 0, 1);

    return 0;
}

int WIIU_SDL_RenderFillRects(SDL_Renderer * renderer, const SDL_FRect * rects, int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;

    /* Compute vertex pos */
    float swidth = data->cbuf.surface.width;
    float sheight = data->cbuf.surface.height;
    float vx = (float)renderer->viewport.x;
    float vy = (float)renderer->viewport.y;

    float *a_position = WIIU_AllocRenderData(data, sizeof(float) * 8 * count);
    for (int i = 0; i < count; ++i) {
        a_position[i*8+0] = vx + rects[i].x;
        a_position[i*8+1] = vy + rects[i].y;
        a_position[i*8+2] = vx + rects[i].x + rects[i].w;
        a_position[i*8+3] = vy + rects[i].y;
        a_position[i*8+4] = vx + rects[i].x + rects[i].w;
        a_position[i*8+5] = vy + rects[i].y + rects[i].h;
        a_position[i*8+6] = vx + rects[i].x;
        a_position[i*8+7] = vy + rects[i].y + rects[i].h;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, a_position, sizeof(float) * 8 * count);

    /* Render rects */
    float u_color[4] = {(float)renderer->r / 255.0f,
                        (float)renderer->g / 255.0f,
                        (float)renderer->b / 255.0f,
                        (float)renderer->a / 255.0f};

    GX2SetContextState(data->ctx);
    wiiuSetColorShader();
    GX2SetAttribBuffer(0, sizeof(float) * 8 * count, sizeof(float) * 2, a_position);
    GX2SetVertexUniformReg(wiiuColorShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)data->u_viewSize);
    GX2SetPixelUniformReg(wiiuColorShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_color);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4 * count, 0, 1);

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

    WIIU_FreeRenderData(data);
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
