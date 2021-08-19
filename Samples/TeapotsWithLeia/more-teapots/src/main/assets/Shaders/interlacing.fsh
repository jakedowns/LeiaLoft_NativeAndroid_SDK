
#version 300 es

uniform float debug;
uniform float width; // landscape is 640.0, portrait should be 360.0
uniform float height; // landscape is 360.0, portrait should be 640.0
uniform sampler2D views[4];
in highp vec2 v_tex;


out vec4 final_color;

void main()
{ 
    if (debug < 0.5)
	{
        final_color = vec4(1.0, 0.0, 0.0, 1.0);
	}
	else if (debug < 1.5)
	{
	    final_color = texture(views[0], v_tex);
	}
	else
	{
	    float view_id = mod(floor(gl_FragCoord.x), 4.0);
		float tex_x = floor(gl_FragCoord.x / 4.0);
		float tex_y = floor(gl_FragCoord.y / 4.0);
		vec2 tex_coord = vec2(tex_x / width, tex_y / height);
		if (view_id < 0.5) { final_color = texture(views[0], v_tex); }
		else if (view_id < 1.5) { final_color = texture(views[1], v_tex); }
		else if (view_id < 2.5) { final_color = texture(views[2], v_tex); }
		else { final_color = texture(views[3], v_tex); }
	}
}