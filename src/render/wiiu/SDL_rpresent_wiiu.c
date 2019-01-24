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

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

static const float u_viewSize[4] = {(float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};
static const float u_mod[4] = {1.0f, 1.0f, 1.0f, 1.0f};

//TODO
static const float a_position[2 * 4] = {
    0.0f, 0.0f,
    1280.0f, 0.0f,
    1280.0f, 720.0f,
    0.0f, 720.0f,
};

static const float a_texCoorssd[] =
{
    0.0f,               (float)SCREEN_HEIGHT,
    (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
    (float)SCREEN_WIDTH, 0.0f,
    0.0f,               0.0f,
};

static void render_scene(WIIU_RenderData *data) {
    WIIU_TextureData *tdata = (WIIU_TextureData *) data->windowTex.driverdata;
    float tex_w = tdata->u_texSize[0];
    float tex_h = tdata->u_texSize[1];

    float* a_texCoord = WIIU_AllocRenderData(data, sizeof(float) * 2 * 4);
    float a_texCoord_vals[] = {
        0.0f, tex_h,
        tex_w, tex_h,
        tex_w, 0.0f,
        0.0f, 0.0f,
    };
    memcpy(a_texCoord, a_texCoord_vals, sizeof(float) * 2 * 4);

    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    wiiuSetTextureShader();

    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)tdata->u_texSize);
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_mod);

    GX2SetAttribBuffer(0, sizeof(float) * 8, sizeof(float) * 2, (void*)a_position);
    GX2SetAttribBuffer(1, sizeof(float) * 8, sizeof(float) * 2, (void*)a_texCoord);

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
