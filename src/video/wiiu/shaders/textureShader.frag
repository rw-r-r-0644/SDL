/* textureShader: fragment (pixel) shader (GLSL) */
/* compile with ShaderAnalyzer (rv770) + fix latte-assembler messages */
/* NOTE: Ash knows R600 but not GLSL, so the .psh may have changes not in this
   file. */

layout(location = 0) uniform sampler2D s_texture; /* texture sampler */
in vec2 texCoord; /* relative texture postion passed from vertex shader */

void main()
{
    /* Compute fragment color for texture/position */
    gl_FragColor = texture2D(s_texture, texCoord);
}
