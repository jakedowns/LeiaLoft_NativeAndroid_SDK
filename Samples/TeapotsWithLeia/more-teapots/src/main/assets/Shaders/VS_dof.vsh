
#version 300 es

in highp vec2 myVertex;
in highp vec2 myUV;

out highp vec2 tc;

void main(void)
{
    gl_Position = vec4(myVertex, 0.0, 1.0);

    tc = myUV;
}
