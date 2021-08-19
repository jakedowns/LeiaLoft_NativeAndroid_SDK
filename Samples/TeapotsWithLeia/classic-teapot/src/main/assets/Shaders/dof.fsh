
#version 300 es

precision highp float;

uniform sampler2DArray colorTex;
uniform sampler2DArray depthTex;

uniform float aspect_ratio;

uniform float view_width;
uniform float aperture;
uniform float convergence_distance;
uniform float f_in_pixels;
uniform float baseline;
uniform float near;
uniform float far;
in vec2 tc;
flat in int tex_id;

out vec4 final_color;

#define DITHERING_FACTOR 0.5

const int kernel_size = 16;
const vec2[] kernel = vec2[](   // Poisson disk from Unity shader
  vec2( 0.00000000, 0.00000000), vec2( 0.54545456, 0.00000000),
  vec2( 0.16855472, 0.51875810), vec2(-0.44128203, 0.32061010),
  vec2(-0.44128197,-0.32061020), vec2( 0.16855480,-0.51875810),
  vec2( 1.00000000, 0.00000000), vec2( 0.80901700, 0.58778524),
  vec2( 0.30901697, 0.95105654), vec2(-0.30901703, 0.95105650),
  vec2(-0.80901706, 0.58778520), vec2(-1.00000000, 0.00000000),
  vec2(-0.80901694,-0.58778536), vec2(-0.30901664,-0.95105660),
  vec2( 0.30901712,-0.95105650), vec2( 0.80901694,-0.58778530)
);
const float[] weights = float[]( // Gaussian kernel for above coordinates
  0.16097572005690314,  0.10087772702157849,  0.10087772972724006,
  0.100877728733224,    0.10087772696353085,  0.10087772545381421,
  0.033463564488187894, 0.03346356476936258,  0.033463562909953504,
  0.03346356496009724,  0.03346356213802727,  0.033463564488187894,
  0.033463562457240095, 0.033463567631518164, 0.03346356203630138,
  0.03346356616483319
);

float real_z(vec2 uv)
{
    float z_b = texture(depthTex, vec3(uv, tex_id)).r;
    float z_n = 2.0 * z_b - 1.0;
    float z_e = 2.0 * near * far / (far + near - z_n * (far - near));
    return z_e;
}

// new function:
// Returning blur map in fraction of screen width
float getBlurInTexelSpace(vec2 uv) {
    float z = real_z(uv);
    float inv_focal_distance = 1.0 / convergence_distance;
    float inv_z = 1.0 / z;
    float disparity_in_pixels = baseline * f_in_pixels * (inv_focal_distance - inv_z);
    float blur = aperture * (disparity_in_pixels / view_width);
    return blur;
}

vec4 getColor(vec2 uv) {
    return texture(colorTex, vec3(uv, tex_id));
}

float rand(float n) {
    return fract(sin(n) * 1784358.5453123) - 0.5;
}

vec2 getDitheringOffset(vec2 uv, float iteration) {
    uv += uv * iteration;
    return DITHERING_FACTOR * vec2(rand(uv.x - uv.x * uv.y), rand(uv.y -  uv.y * uv.x));
}

vec2 circleToEllipse(vec2 uv) {
    return vec2(uv.x, uv.y / aspect_ratio);
}

vec4 dof(vec2 uv) {

    vec4 result = vec4(0.0);
    float blur_radius = getBlurInTexelSpace(uv);
    for (int i = 0; i < kernel_size; i++) {
        vec2 point = circleToEllipse(kernel[i] + getDitheringOffset(uv, float(i)));
        vec2 new_uv = uv + blur_radius * point;
        vec4 new_color = getColor(new_uv) * weights[i];
       result += new_color;
    }
    return result;
}

void main(void) {
    final_color = dof(tc);
}