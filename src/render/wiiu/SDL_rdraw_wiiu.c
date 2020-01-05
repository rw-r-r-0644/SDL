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

int WIIU_SDL_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                       const SDL_Rect * srcrect, const SDL_FRect * dstrect) {
    WIIUVec2 *a_position_vals, *a_texCoord_vals;
    float x_min, y_min, x_max, y_max;

/*  Compute vertex points */
    x_min = renderer->viewport.x + dstrect->x;
    y_min = renderer->viewport.y + dstrect->y;
    x_max = renderer->viewport.x + dstrect->x + dstrect->w;
    y_max = renderer->viewport.y + dstrect->y + dstrect->h;

/*  We are INTERLEAVING the vertexes in this buffer.
    1 a_position vector, 1 a_texCoord vector, repeat.
    SDL only lets us do one allocation per render command and interleaving
    makes performance sense here */
    cmd->data.draw.count = 4 /* corners */ * 2 /* attribte buffers */;
    a_position_vals = (WIIUVec2*) SDL_AllocateRenderVertices(renderer,
        sizeof(WIIUVec2) /* float x/y for each corner */
            * cmd->data.draw.count,
        4, //TODO align? GX2R uses 256 (!)
        &cmd->data.draw.first
    );
    a_texCoord_vals = a_position_vals + 1;

/*  Save vertex points (*2 are because interleaving) */
    a_position_vals[0 *2] = (WIIUVec2){.x = x_min, .y = y_min};
    a_position_vals[1 *2] = (WIIUVec2){.x = x_max, .y = y_min};
    a_position_vals[2 *2] = (WIIUVec2){.x = x_max, .y = y_max};
    a_position_vals[3 *2] = (WIIUVec2){.x = x_min, .y = y_max};

/*  Save texture coordinates (*2 are because interleaving) */
    a_texCoord_vals[0 *2] = (WIIUVec2) {
        .x = srcrect->x,
        .y = srcrect->y + srcrect->h,
    };
    a_texCoord_vals[1 *2] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = srcrect->y + srcrect->h,
    };
    a_texCoord_vals[2 *2] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = srcrect->y,
    };
    a_texCoord_vals[3 *2] = (WIIUVec2) {
        .x = srcrect->x,
        .y = srcrect->y,
    };

    return 0;
}

int WIIU_SDL_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                         const SDL_Rect *srcrect, const SDL_FRect *dstrect,
                         const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip) {
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
            .y = (flip & SDL_FLIP_VERTICAL) ? y_max : y_min,
        },
        {
            .x = (flip & SDL_FLIP_HORIZONTAL) ? x_min : x_max,
            .y = (flip & SDL_FLIP_VERTICAL) ? y_max : y_min,
        },
        {
            .x = (flip & SDL_FLIP_HORIZONTAL) ? x_min : x_max,
            .y = (flip & SDL_FLIP_VERTICAL) ? y_min : y_max,
        },
        {
            .x = (flip & SDL_FLIP_HORIZONTAL) ? x_max : x_min,
            .y = (flip & SDL_FLIP_VERTICAL) ? y_min : y_max,
        },
    };

/*  We are INTERLEAVING the vertexes in this buffer.
    1 a_position vector, 1 a_texCoord vector, repeat.
    SDL only lets us do one allocation per render command and interleaving
    makes performance sense here */
    cmd->data.draw.count = 4 /* corners */ * 2 /* attribte buffers */;
    a_position_vals = (WIIUVec2*) SDL_AllocateRenderVertices(renderer,
        sizeof(WIIUVec2) /* float x/y for each corner */
            * cmd->data.draw.count,
        4, //TODO align? GX2R uses 256 (!)
        &cmd->data.draw.first
    );
    a_texCoord_vals = a_position_vals + 1;

/*  Save vertex points */
    for (int i = 0; i < 4; i++) {
        a_position_vals[i *2] = (WIIUVec2) {
            .x = cx + (SDL_cos(r) * (rvb[i].x - cx) - SDL_sin(r) * (rvb[i].y - cy)),
            .y = cy + (SDL_cos(r) * (rvb[i].y - cy) + SDL_sin(r) * (rvb[i].x - cx)),
        };
    }

/*  Save texture coordinates (*2 are because interleaving) */
    a_texCoord_vals[0 *2] = (WIIUVec2) {
        .x = srcrect->x,
        .y = srcrect->y + srcrect->h,
    };
    a_texCoord_vals[1 *2] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = srcrect->y + srcrect->h,
    };
    a_texCoord_vals[2 *2] = (WIIUVec2) {
        .x = srcrect->x + srcrect->w,
        .y = srcrect->y,
    };
    a_texCoord_vals[3 *2] = (WIIUVec2) {
        .x = srcrect->x,
        .y = srcrect->y,
    };

    return 0;
}

/*  This function is identical for Copy and CopyEx */
void WIIU_SDL_RenderCopy(SDL_Renderer * renderer, SDL_RenderCommand* cmd, void* vertexes)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) cmd->data.draw.texture->driverdata;
    WIIUVec2 *a_position_vals, *a_texCoord_vals;
    size_t count, stride;

/*  Set up pointers for deinterleaving */
    a_position_vals = (WIIUVec2*)vertexes;
    a_texCoord_vals = a_position_vals + 1;
    count = cmd->data.draw.count / 2;
    stride = sizeof(a_position_vals[0]) * 2;

    if (cmd->data.draw.texture->access & SDL_TEXTUREACCESS_TARGET) {
        GX2RInvalidateSurface(&tdata->texture.surface, 0, 0);
    }

    /* Update texture rendering state */
    WIIU_TextureStartRendering(data, tdata);

/*  Invalidate caches related to vertexes */
    GX2Invalidate(
        GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER,
        vertexes,
        count * stride
    );

    /* Render */
    wiiuSetTextureShader();
    GX2SetPixelTexture(&tdata->texture, 0);
    GX2SetPixelSampler(&tdata->sampler, 0);
    GX2SetAttribBuffer(0, count * stride, stride, a_position_vals);
    GX2SetAttribBuffer(1, count * stride, stride, a_texCoord_vals); //TODO runs off end of buffer lol
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&data->u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)&tdata->u_texSize);
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t *)&tdata->u_mod);
    WIIU_SDL_SetGX2BlendMode(cmd->data.draw.blend);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

int WIIU_SDL_QueueDrawPointsLines(SDL_Renderer* renderer, SDL_RenderCommand* cmd, const SDL_FPoint* points, int count) {
    WIIUVec2 *a_position_vals;

    cmd->data.draw.count = count;
    a_position_vals = SDL_AllocateRenderVertices(renderer,
        sizeof (WIIUVec2) /* float x/y for each point */
            * cmd->data.draw.count,
        4, //TODO align
        &cmd->data.draw.first
    );

    for (int i = 0; i < count; ++i) {
        a_position_vals[i] = (WIIUVec2) {
        /*  TODO this code used to add the viewport x/y, this is broken with
            batching. Check if that behaviour was correct */
            .x = points[i].x,
            .y = points[i].y,
        };
    }

    return 0;
}

int WIIU_SDL_QueueFillRects(SDL_Renderer* renderer, SDL_RenderCommand* cmd, const SDL_FRect* rects, int count) {
    WIIUVec2 *a_position_vals;

    cmd->data.draw.count = count * 4 /* 4 vertexes per rect */;
    a_position_vals = SDL_AllocateRenderVertices(renderer,
        sizeof (WIIUVec2) /* float x/y for each point */
            * cmd->data.draw.count,
        4, //TODO align
        &cmd->data.draw.first
    );

    for (int i = 0; i < count; ++i) {
        a_position_vals[i*4 + 0] = (WIIUVec2) {
        /*  TODO this code used to add the viewport x/y, this is broken with
            batching. Check if that behaviour was correct */
            .x = rects[i].x,
            .y = rects[i].y,
        };
        a_position_vals[i*4 + 1] = (WIIUVec2) {
            .x = rects[i].x + rects[i].w,
            .y = rects[i].y,
        };
        a_position_vals[i*4 + 2] = (WIIUVec2) {
            .x = rects[i].x + rects[i].w,
            .y = rects[i].y + rects[i].h,
        };
        a_position_vals[i*4 + 3] = (WIIUVec2) {
            .x = rects[i].x,
            .y = rects[i].y + rects[i].h,
        };
    }

    return 0;
}

void WIIU_SDL_RenderDrawPrimitive(SDL_Renderer * renderer, SDL_RenderCommand* cmd, void* vertexes) {
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    size_t count, stride;

    WIIUVec4 u_colour = {
        .r = (float)cmd->data.draw.r / 255.0f,
        .g = (float)cmd->data.draw.g / 255.0f,
        .b = (float)cmd->data.draw.b / 255.0f,
        .a = (float)cmd->data.draw.a / 255.0f,
    };

    count = cmd->data.draw.count;
    stride = sizeof(WIIUVec2);

/*  Invalidate caches related to vertexes */
    GX2Invalidate(
        GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER,
        vertexes,
        count * stride
    );

    /* Render points */
    wiiuSetColorShader();
    GX2SetAttribBuffer(0, count * stride, stride, vertexes);
    GX2SetVertexUniformReg(wiiuColorShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&data->u_viewSize);
    GX2SetPixelUniformReg(wiiuColorShader.pixelShader->uniformVars[0].offset, 4, (uint32_t *)&u_colour);
    WIIU_SDL_SetGX2BlendMode(cmd->data.draw.blend);
    if (cmd->command == SDL_RENDERCMD_DRAW_POINTS) {
        GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, count, 0, 1);
    } else if (cmd->command == SDL_RENDERCMD_DRAW_LINES) {
        GX2DrawEx(GX2_PRIMITIVE_MODE_LINE_STRIP, count, 0, 1);
    } else if (cmd->command == SDL_RENDERCMD_FILL_RECTS) {
        GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, count, 0, 1);
    }
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
