
#version 300 es
#extension GL_OVR_multiview2 : enable
layout(num_views = 4) in;

in highp vec2 myVertex;
in highp vec2 myUV;

out highp vec2 tc;
flat out int tex_id;

void main(void)
{
    gl_Position = vec4(myVertex, 0.0, 1.0);
    tex_id = int(gl_ViewID_OVR);
    tc = myUV;
}
