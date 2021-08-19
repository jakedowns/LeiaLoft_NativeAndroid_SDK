
#version 300 es

in highp vec2 myVertex;
in highp vec2 myUV;

uniform highp mat4 translation;
uniform highp mat4 cam;
uniform highp mat4 perspective;

out highp vec2 v_tex;

void main(void)
{
    gl_Position = perspective * cam * translation * vec4(myVertex, 0.0, 1.0);

    v_tex = myUV;
}
