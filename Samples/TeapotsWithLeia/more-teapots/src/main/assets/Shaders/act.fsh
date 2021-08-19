
#version 300 es

uniform float debug;
uniform float width; // landscape is 2560.0, portrait should be 1440.0
uniform float a; // 0.065 is a current decent value
uniform float b; // 0.03 is a current decent value
uniform sampler2D interlaced_view;
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
	    final_color = texture(interlaced_view, v_tex);
	}
	else
	{
        vec4 far_left = texture(interlaced_view, v_tex + vec2(-2.0, 0.0) * vec2(1.0 / width, 0.0));
        vec4 left = texture(interlaced_view, v_tex + vec2(-1.0, 0.0) * vec2(1.0 / width, 0.0));
        vec4 center = texture(interlaced_view, v_tex + vec2(0.0, 0.0) * vec2(1.0 / width, 0.0));
        vec4 right = texture(interlaced_view, v_tex + vec2(1.0, 0.0) * vec2(1.0 / width, 0.0));
        vec4 far_right = texture(interlaced_view, v_tex + vec2(2.0, 0.0) * vec2(1.0 / width, 0.0));

        float multiplier = 1.0 - (2.0 * a) - (2.0 * b);
        vec4 linear_far_left = b * pow(far_left, vec4(2.0));
        vec4 linear_left = a * pow(left, vec4(2.0));
        vec4 linear_center = pow(center, vec4(2.0));
        vec4 linear_right = a * pow(right, vec4(2.0));
        vec4 linear_far_right = b * pow(far_right, vec4(2.0));

        vec4 clamped = clamp((linear_center -
                              linear_far_left -
                              linear_left -
                              linear_right -
                              linear_far_right) / multiplier, 0.0, 1.0);
        final_color = sqrt(clamped);
	}
}