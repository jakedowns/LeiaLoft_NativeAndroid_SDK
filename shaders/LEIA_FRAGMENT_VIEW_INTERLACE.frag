#version 300 es

uniform sampler2D view_far_left;
uniform sampler2D view_left;
uniform sampler2D view_right;
uniform sampler2D view_far_right;
uniform float alignment_offset;
uniform float debug;
in highp vec2 v_tex;


out vec4 final_color;

void main()
{ 
    float view_id = mod(floor(gl_FragCoord.x + alignment_offset), 4.0);
    if (view_id < 0.5) { final_color = texture(view_far_left, v_tex); }
    else if (view_id < 1.5) { final_color = texture(view_left, v_tex); }
    else if (view_id < 2.5) { final_color = texture(view_right, v_tex); }
    else { final_color = texture(view_far_right, v_tex); }
}
