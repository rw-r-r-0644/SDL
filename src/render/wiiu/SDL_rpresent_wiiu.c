#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_WIIU

#include "../../video/wiiu/SDL_wiiuvideo.h"
#include "../../video/wiiu/wiiu_shaders.h"
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include "SDL_render_wiiu.h"

#include <whb/gfx.h>
#include <gx2/draw.h>
#include <gx2/state.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

static const float u_viewSize[4] = {(float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};

static void render_scene(WIIU_RenderData *data) {
    WIIU_TextureData *tdata = (WIIU_TextureData *) data->windowTex.driverdata;
    float tex_w = tdata->u_texSize[0];
    float tex_h = tdata->u_texSize[1];
    GX2RBuffer *a_position, *a_texCoord;
    float *a_position_vals, *a_texCoord_vals;
    float u_mod[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /*  Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = 2 * sizeof(float), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });
    a_texCoord = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = 2 * sizeof(float), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });

/*  Save them */
    a_position_vals = GX2RLockBufferEx(a_position, 0);
/*  TODO: not 720p. also, this is legal C? */
    memcpy(
        a_position_vals,
        (float[]) {
            0.0f, 0.0f,
            1280.0f, 0.0f,
            1280.0f, 720.0f,
            0.0f, 720.0f,
        },
        a_position->elemSize * a_position->elemCount
    );
    GX2RUnlockBufferEx(a_position, 0);

/*  Compute texture coords */
    a_texCoord_vals = GX2RLockBufferEx(a_texCoord, 0);
    memcpy(
        a_texCoord_vals,
        (float[]) {
            0.0f, tex_h,
            tex_w, tex_h,
            tex_w, 0.0f,
            0.0f, 0.0f,
        },
        a_texCoord->elemSize * a_position->elemCount
    );
    GX2RUnlockBufferEx(a_position, 0);

    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    wiiuSetTextureShader();

    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)tdata->u_texSize);
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_mod);

    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2RSetAttributeBuffer(a_texCoord, 1, a_texCoord->elemSize, 0);

    GX2SetPixelTexture(&tdata->texture, wiiuTextureShader.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&tdata->sampler, wiiuTextureShader.pixelShader->samplerVars[0].location);

    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

void WIIU_SDL_RenderPresent(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    //SDL_Window *window = renderer->window;

    //GX2Flush();

    WHBGfxBeginRender();

    WHBGfxBeginRenderTV();
    render_scene(data);
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    render_scene(data);
    WHBGfxFinishRenderDRC();

    WHBGfxFinishRender();

    WIIU_FreeRenderData(data);
}

#endif //SDL_VIDEO_RENDER_WIIU
