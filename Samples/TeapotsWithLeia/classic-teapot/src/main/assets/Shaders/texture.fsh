
#version 300 es

uniform sampler2D tex_sampler;
in highp vec2 v_tex;

out vec4 final_color;

void main()
{
    final_color = texture(tex_sampler, v_tex);
}
