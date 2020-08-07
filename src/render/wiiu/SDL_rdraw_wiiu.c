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

#include <gx2/texture.h>
#include <gx2/draw.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/state.h>
#include <gx2/clear.h>
#include <gx2/mem.h>
#include <gx2/event.h>
#include <gx2r/surface.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void WIIU_SDL_SetGX2BlendMode(SDL_BlendMode mode);

int WIIU_SDL_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                        const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;
    GX2RBuffer *a_position, *a_texCoord;
    WIIUVec2 *a_position_vals, *a_texCoord_vals;
    float x_min, y_min, x_max, y_max;

    if (texture->access & SDL_TEXTUREACCESS_TARGET) {
        GX2RInvalidateSurface(&tdata->texture.surface, 0, 0);
    }

    /* Update texture rendering state */
    WIIU_TextureStartRendering(data, tdata);

    /* Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });
    a_texCoord = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2),
        .elemCount = 4, // 4 corners
    });

    /* Compute vertex points */
    x_min = renderer->viewport.x + dstrect->x;
    y_min = renderer->viewport.y + dstrect->y;
    x_max = renderer->viewport.x + dstrect->x + dstrect->w;
    y_max = renderer->viewport.y + dstrect->y + dstrect->h;

    /* Save them */
    a_position_vals = GX2RLockBufferEx(a_position, 0);
    a_position_vals[0] = (WIIUVec2){.x = x_min, .y = y_max};
    a_position_vals[1] = (WIIUVec2){.x = x_max, .y = y_max};
    a_position_vals[2] = (WIIUVec2){.x = x_max, .y = y_min};
    a_position_vals[3] = (WIIUVec2){.x = x_min, .y = y_min};
    GX2RUnlockBufferEx(a_position, 0);

    /* Compute texture coords */
    a_texCoord_vals = GX2RLockBufferEx(a_texCoord, 0);
    a_texCoord_vals[0] = (WIIUVec2) {
        .x = srcrect->x,
        .y = texture->h - srcrect->y - srcrect->h,
    };
    a_texCoord_vals[1] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = texture->h - srcrect->y - srcrect->h,
    };
    a_texCoord_vals[2] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = texture->h - srcrect->y,
    };
    a_texCoord_vals[3] = (WIIUVec2) {
        .x = srcrect->x,
        .y = texture->h - srcrect->y,
    };
    GX2RUnlockBufferEx(a_texCoord, 0);

    /* Render */
    wiiuSetTextureShader();
    GX2SetPixelTexture(&tdata->texture, 0);
    GX2SetPixelSampler(&tdata->sampler, 0);
    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2RSetAttributeBuffer(a_texCoord, 1, a_texCoord->elemSize, 0);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&data->u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)&tdata->u_texSize);
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t *)&tdata->u_mod);
    WIIU_SDL_SetGX2BlendMode(texture->blendMode);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

    return 0;
}


int WIIU_SDL_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                          const double angle, const SDL_FPoint * center, const SDL_RendererFlip flip)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) texture->driverdata;
    GX2RBuffer *a_position, *a_texCoord;
    WIIUVec2 *a_position_vals, *a_texCoord_vals;

    /* Compute real vertex points */
    float x_min = renderer->viewport.x + dstrect->x;
    float y_min = renderer->viewport.y + dstrect->y;
    float x_max = x_min + dstrect->w;
    float y_max = y_min + dstrect->h;
    float cx = x_min + center->x;
    float cy = y_min + center->y;
    double r = angle * (M_PI / 180.0);
    WIIUVec2 rvb[4] = {
        {
            .x = (flip & SDL_FLIP_HORIZONTAL) ? x_max : x_min,
            .y = (flip & SDL_FLIP_VERTICAL) ? y_min : y_max,
        },
        {
            .x = (flip & SDL_FLIP_HORIZONTAL) ? x_min : x_max,
            .y = (flip & SDL_FLIP_VERTICAL) ? y_min : y_max,
        },
        {
            .x = (flip & SDL_FLIP_HORIZONTAL) ? x_min : x_max,
            .y = (flip & SDL_FLIP_VERTICAL) ? y_max : y_min,
        },
        {
            .x = (flip & SDL_FLIP_HORIZONTAL) ? x_max : x_min,
            .y = (flip & SDL_FLIP_VERTICAL) ? y_max : y_min,
        },
    };

    if (texture->access & SDL_TEXTUREACCESS_TARGET) {
        GX2RInvalidateSurface(&tdata->texture.surface, 0, 0);
    }

    /* Update texture rendering state */
    WIIU_TextureStartRendering(data, tdata);

    /* Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });
    a_texCoord = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });

    /* Save vertex points */
    a_position_vals = GX2RLockBufferEx(a_position, 0);
    for (int i = 0; i < 4; i++) {
        a_position_vals[i] = (WIIUVec2) {
            .x = cx + (SDL_cos(r) * (rvb[i].x - cx) - SDL_sin(r) * (rvb[i].y - cy)),
            .y = cy + (SDL_cos(r) * (rvb[i].y - cy) + SDL_sin(r) * (rvb[i].x - cx)),
        };
    }
    GX2RUnlockBufferEx(a_position, 0);

    /* Compute texture coords */
    a_texCoord_vals = GX2RLockBufferEx(a_texCoord, 0);
    a_texCoord_vals[0] = (WIIUVec2) {
        .x = srcrect->x,
        .y = texture->h - srcrect->y - srcrect->h,
    };
    a_texCoord_vals[1] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = texture->h - srcrect->y - srcrect->h,
    };
    a_texCoord_vals[2] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = texture->h - srcrect->y,
    };
    a_texCoord_vals[3] = (WIIUVec2) {
        .x = srcrect->x,
        .y = texture->h - srcrect->y,
    };
    GX2RUnlockBufferEx(a_texCoord, 0);

    /* Render */
    wiiuSetTextureShader();
    GX2SetPixelTexture(&tdata->texture, 0);
    GX2SetPixelSampler(&tdata->sampler, 0);
    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2RSetAttributeBuffer(a_texCoord, 1, a_texCoord->elemSize, 0);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&data->u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)&tdata->u_texSize);
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t *)&tdata->u_mod);
    WIIU_SDL_SetGX2BlendMode(texture->blendMode);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

    return 0;
}

int WIIU_SDL_RenderDrawPoints(SDL_Renderer * renderer, const SDL_FPoint * points,
                              int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2RBuffer *a_position;
    WIIUVec2 *a_position_vals;

    /* Compute colours */
    WIIUVec4 u_colour = {
        .r = (float)renderer->r / 255.0f,
        .g = (float)renderer->g / 255.0f,
        .b = (float)renderer->b / 255.0f,
        .a = (float)renderer->a / 255.0f,
    };

    /* Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), /* float x/y for each point */
        .elemCount = count,
    });

    /* Compute vertex positions */
    a_position_vals = GX2RLockBufferEx(a_position, 0);
    for (int i = 0; i < count; ++i) {
        a_position_vals[i] = (WIIUVec2) {
            .x = (float)renderer->viewport.x + points[i].x,
            .y = (float)renderer->viewport.y + points[i].y,
        };
    }
    GX2RUnlockBufferEx(a_position, 0);

    /* Render points */
    wiiuSetColorShader();
    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2SetVertexUniformReg(wiiuColorShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&data->u_viewSize);
    GX2SetPixelUniformReg(wiiuColorShader.pixelShader->uniformVars[0].offset, 4, (uint32_t *)&u_colour);
    WIIU_SDL_SetGX2BlendMode(renderer->blendMode);
    GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, count, 0, 1);

    return 0;
}


int WIIU_SDL_RenderDrawLines(SDL_Renderer * renderer, const SDL_FPoint * points,
                             int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2RBuffer *a_position;
    WIIUVec2 *a_position_vals;

    /* Compute colours */
    WIIUVec4 u_colour = {
        .r = (float)renderer->r / 255.0f,
        .g = (float)renderer->g / 255.0f,
        .b = (float)renderer->b / 255.0f,
        .a = (float)renderer->a / 255.0f,
    };

    /*  Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), /* float x/y for each point */
        .elemCount = count,
    });

    /* Compute vertex positions */
    a_position_vals = GX2RLockBufferEx(a_position, 0);
    for (int i = 0; i < count; ++i) {
        a_position_vals[i] = (WIIUVec2) {
            .x = (float)renderer->viewport.x + points[i].x,
            .y = (float)renderer->viewport.y + points[i].y,
        };
    }
    GX2RUnlockBufferEx(a_position, 0);

    /* Render lines */
    wiiuSetColorShader();
    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2SetVertexUniformReg(wiiuColorShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&data->u_viewSize);
    GX2SetPixelUniformReg(wiiuColorShader.pixelShader->uniformVars[0].offset, 4, (uint32_t *)&u_colour);
    WIIU_SDL_SetGX2BlendMode(renderer->blendMode);
    GX2DrawEx(GX2_PRIMITIVE_MODE_LINE_STRIP, count, 0, 1);

    return 0;
}

int WIIU_SDL_RenderFillRects(SDL_Renderer * renderer, const SDL_FRect * rects, int count)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    GX2RBuffer *a_position;
    WIIUVec2 *a_position_vals;

    /* Compute colours */
    WIIUVec4 u_colour = {
        .r = (float)renderer->r / 255.0f,
        .g = (float)renderer->g / 255.0f,
        .b = (float)renderer->b / 255.0f,
        .a = (float)renderer->a / 255.0f,
    };

    /* Compute vertex pos */
    float vx = (float)renderer->viewport.x;
    float vy = (float)renderer->viewport.y;

    /*  Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), // x/y float per corner
        .elemCount = 4 * count, // 4 corners per square
    });

    /* Compute vertex positions */
    a_position_vals = GX2RLockBufferEx(a_position, 0);
    for (int i = 0; i < count; ++i) {
        a_position_vals[i*4 + 0] = (WIIUVec2) {
            .x = vx + rects[i].x,
            .y = vy + rects[i].y,
        };
        a_position_vals[i*4 + 1] = (WIIUVec2) {
            .x = vx + rects[i].x + rects[i].w,
            .y = vy + rects[i].y,
        };
        a_position_vals[i*4 + 2] = (WIIUVec2) {
            .x = vx + rects[i].x + rects[i].w,
            .y = vy + rects[i].y + rects[i].h,
        };
        a_position_vals[i*4 + 3] = (WIIUVec2) {
            .x = vx + rects[i].x,
            .y = vy + rects[i].y + rects[i].h,
        };
    }
    GX2RUnlockBufferEx(a_position, 0);

    /* Render rects */
    wiiuSetColorShader();
    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2SetVertexUniformReg(wiiuColorShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&data->u_viewSize);
    GX2SetPixelUniformReg(wiiuColorShader.pixelShader->uniformVars[0].offset, 4, (uint32_t *)&u_colour);
    WIIU_SDL_SetGX2BlendMode(renderer->blendMode);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4 * count, 0, 1);

    return 0;
}

int WIIU_SDL_RenderClear(SDL_Renderer * renderer)
{
    WIIU_RenderData* data = (WIIU_RenderData*) renderer->driverdata;
    SDL_Texture* target = WIIU_GetRenderTarget(renderer);
    WIIU_TextureData* tdata = (WIIU_TextureData*) target->driverdata;

    GX2ClearColor(&tdata->cbuf,
                  (float)renderer->r / 255.0f,
                  (float)renderer->g / 255.0f,
                  (float)renderer->b / 255.0f,
                  (float)renderer->a / 255.0f);

    /* Restore SDL context state */
    GX2SetContextState(data->ctx);

    return 0;
}

static void WIIU_SDL_SetGX2BlendMode(SDL_BlendMode mode)
{
    if (mode == SDL_BLENDMODE_NONE) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE);
    } else if (mode == SDL_BLENDMODE_BLEND) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
            /* RGB = [srcRGB * srcA] + [dstRGB * (1-srcA)] */
            GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
            GX2_BLEND_COMBINE_MODE_ADD,
            TRUE,
            /* A = [srcA * 1] + [dstA * (1-srcA)] */
            GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA,
            GX2_BLEND_COMBINE_MODE_ADD);
    } else if (mode == SDL_BLENDMODE_ADD) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
            /* RGB = [srcRGB * srcA] + [dstRGB * 1] */
            GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_ONE,
            GX2_BLEND_COMBINE_MODE_ADD,
            TRUE,
            /* A = [srcA * 0] + [dstA * 1] */
            GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ONE,
            GX2_BLEND_COMBINE_MODE_ADD);
    } else if (mode == SDL_BLENDMODE_MOD) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
            /* RGB = [srcRGB * dstRGB] + [dstRGB * 0]) */
            GX2_BLEND_MODE_DST_COLOR, GX2_BLEND_MODE_ZERO,
            GX2_BLEND_COMBINE_MODE_ADD,
            TRUE,
            /* A = [srcA * 0] + [dstA * 1] */
            GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ONE,
            GX2_BLEND_COMBINE_MODE_ADD);
    }
}

#endif /* SDL_VIDEO_RENDER_WIIU */
