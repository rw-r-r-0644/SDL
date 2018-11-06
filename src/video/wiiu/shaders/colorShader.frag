/* colorShader: fragment (pixel) shader (GLSL) */
/* compile with ShaderAnalyzer (rv770) + fix latte-assembler messages */

layout(location = 0) uniform vec4 u_color; /* color */

void main()
{
    /* Set fragment color */
    gl_FragColor = u_color;
}
