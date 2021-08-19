
#version 300 es

uniform float debug;
uniform sampler2DArray views;
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
	    final_color = texture(views, vec3(v_tex, 0));
	}
	else
	{
	    float view_id = mod(floor(gl_FragCoord.x), 4.0);
		if (view_id < 0.5) { final_color = texture(views, vec3(v_tex, 0)); }
		else if (view_id < 1.5) { final_color = texture(views, vec3(v_tex, 1)); }
		else if (view_id < 2.5) { final_color = texture(views, vec3(v_tex, 2)); }
		else { final_color = texture(views, vec3(v_tex, 3)); }
	}
}