/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//--------------------------------------------------------------------------------
// Teapot Renderer.h
// Renderer for teapots
//--------------------------------------------------------------------------------
#ifndef _TEAPOTRENDERER_H
#define _TEAPOTRENDERER_H

//--------------------------------------------------------------------------------
// Include files
//--------------------------------------------------------------------------------
#include <jni.h>
#include <errno.h>

#include <vector>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/native_window_jni.h>
#include <cpu-features.h>

#define CLASS_NAME "android/app/NativeActivity"
#define APPLICATION_CLASS_NAME "com/sample/teapot/TeapotApplication"

#include "NDKHelper.h"

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

struct TEAPOT_VERTEX {
    float pos[3];
    float normal[3];
};

enum SHADER_ATTRIBUTES {
    ATTRIB_VERTEX,
    ATTRIB_NORMAL,
    ATTRIB_UV,
};

struct SHADER_PARAMS {
    GLuint program_;
    GLuint light0_;
    GLuint material_diffuse_;
    GLuint material_ambient_;
    GLuint material_specular_;

    GLuint matrix_projection_;
    GLuint matrix_view_;
};

struct TEAPOT_MATERIALS {
    float diffuse_color[3];
    float specular_color[4];
    float ambient_color[3];
};

class TeapotRenderer {
    int32_t num_indices_;
    int32_t num_vertices_;
    GLuint ibo_;
    GLuint vbo_;

    SHADER_PARAMS shader_param_;

    bool LoadShaders(SHADER_PARAMS *params, const char *strVsh,
                     const char *strFsh);

    static const unsigned int sNUM_OBJECTS = 3;
    ndk_helper::Mat4 mat_view_[sNUM_OBJECTS];

    ndk_helper::Mat4 mat_projection_;
    ndk_helper::Mat4 mat_model_;

    ndk_helper::TapCamera *camera_;

    static const int RT_COUNT = 4;
    GLuint fullscreen_fbo;
    GLuint fullscreen_texture;
    GLuint fbos[RT_COUNT];
    GLuint render_textures[RT_COUNT];
    GLuint depth_textures[RT_COUNT];

    GLuint fbo_dof[RT_COUNT];
    GLuint texture_dof[RT_COUNT];
    SHADER_PARAMS dof_shader;

    SHADER_PARAMS view_interlacing_shader;
    SHADER_PARAMS view_sharpening_shader;
    SHADER_PARAMS texture_shader;
    int screen_width_pixels_;
    int screen_height_pixels_;
    int view_width_pixels_;
    int view_height_pixels_;

    const float CAM_NEAR = 5.0f;
    const float CAM_FAR = 10000.0f;

    GLuint checkerboard_texture;

    bool using_simple_leia_rendering_api;
public:
    TeapotRenderer();

    virtual ~TeapotRenderer();

    void Init();

    void PrepareCheckerboard();

    void PrepareFullscreenSurface();

    void PrepareRenderTargetSurfaces();

    void RenderViews(bool is_backlight_still_on);

    void RenderView(unsigned int x, unsigned int y, bool use_leia);

    void Update(float dTime);

    bool Bind(ndk_helper::TapCamera *camera);

    void Unload();

    void UpdateViewport();

    GLuint CreateTexture(int width, int height, GLint internal_format,
                         GLenum format, GLenum type, void *data);

    void DrawTexturedQuad(GLint shader_id);

    void DrawBillboard(float *persp);
};

#define CHECK_GL_ERROR() \
do { \
    int err = glGetError(); \
    if (err != GL_NO_ERROR) \
    { \
        LOGE("GL_ERRORS! %d, 0x%x", __LINE__, err); \
    } \
} \
while(0) \

#endif
