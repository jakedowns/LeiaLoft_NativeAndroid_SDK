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
// MoreTeapotsRenderer.cpp
// Render teapots
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Include files
//--------------------------------------------------------------------------------
#include "MoreTeapotsRenderer.h"

#include <string.h>
#include <LeiaCameraViews.h>
#include <LeiaNativeSDK.h>

//--------------------------------------------------------------------------------
// Teapot model data
//--------------------------------------------------------------------------------
#include "teapot.inl"
#include "LeiaJNIDisplayParameters.h"
#include <cmath>
#include <cstdlib>

const unsigned int CAMERAS_HIGH = 1;
const unsigned int CAMERAS_WIDE = 4;
LeiaCameraView cameras[CAMERAS_HIGH][CAMERAS_WIDE];
LeiaCameraData data;

//--------------------------------------------------------------------------------
// Ctor
//--------------------------------------------------------------------------------
MoreTeapotsRenderer::MoreTeapotsRenderer() {}

//--------------------------------------------------------------------------------
// Dtor
//--------------------------------------------------------------------------------
MoreTeapotsRenderer::~MoreTeapotsRenderer() { Unload(); }

//--------------------------------------------------------------------------------
// Init
//--------------------------------------------------------------------------------
void MoreTeapotsRenderer::Init(const int32_t numX, const int32_t numY,
                               const int32_t numZ) {
    using_simple_leia_rendering_api = false;

    // Settings
    glFrontFace(GL_CCW);

    // Create Index buffer
    num_indices_ = sizeof(teapotIndices) / sizeof(teapotIndices[0]);
    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(teapotIndices), teapotIndices,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Create VBO
    num_vertices_ = sizeof(teapotPositions) / sizeof(teapotPositions[0]) / 3;
    int32_t stride = sizeof(TEAPOT_VERTEX);
    int32_t index = 0;
    TEAPOT_VERTEX *p = new TEAPOT_VERTEX[num_vertices_];
    for (int32_t i = 0; i < num_vertices_; ++i) {
        p[i].pos[0] = teapotPositions[index];
        p[i].pos[1] = teapotPositions[index + 1];
        p[i].pos[2] = teapotPositions[index + 2];

        p[i].normal[0] = teapotNormals[index];
        p[i].normal[1] = teapotNormals[index + 1];
        p[i].normal[2] = teapotNormals[index + 2];
        index += 3;
    }
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, stride * num_vertices_, p, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    delete[] p;

    // Init Projection matrices
    teapot_x_ = numX;
    teapot_y_ = numY;
    teapot_z_ = numZ;
    vec_mat_models_.reserve(teapot_x_ * teapot_y_ * teapot_z_);

    UpdateViewport();

    const float total_width = 500.f;
    float gap_x = total_width / (teapot_x_ - 1);
    float gap_y = total_width / (teapot_y_ - 1);
    float gap_z = total_width / (teapot_z_ - 1);
    float offset_x = -total_width / 2.f;
    float offset_y = -total_width / 2.f;
    float offset_z = -total_width / 2.f;

    for (int32_t x = 0; x < teapot_x_; ++x)
        for (int32_t y = 0; y < teapot_y_; ++y)
            for (int32_t z = 0; z < teapot_z_; ++z) {
                vec_mat_models_.push_back(ndk_helper::Mat4::Translation(
                        x * gap_x + offset_x, y * gap_y + offset_y,
                        z * gap_z + offset_z));
                vec_colors_.push_back(ndk_helper::Vec3(
                        random() / float(RAND_MAX * 1.1), random() / float(RAND_MAX * 1.1),
                        random() / float(RAND_MAX * 1.1)));

                float rotation_x = random() / float(RAND_MAX) - 0.5f;
                float rotation_y = random() / float(RAND_MAX) - 0.5f;
                vec_rotations_.push_back(ndk_helper::Vec2(rotation_x * 0.05f, rotation_y * 0.05f));
                vec_current_rotations_.push_back(
                        ndk_helper::Vec2(rotation_x * M_PI, rotation_y * M_PI));
            }

    unsigned int len = 0;
    LoadShaders(&shader_param_, "Shaders/VS_ShaderPlain.vsh",
                "Shaders/ShaderPlain.fsh");
    dof_shader.program_ = leiaCreateProgram(leiaGetShader(LEIA_VERTEX_DOF, &len),
                                            leiaGetShader(LEIA_FRAGMENT_DOF, &len));
    view_interlacing_shader.program_ = leiaCreateProgram(
            leiaGetShader(LEIA_VERTEX_VIEW_INTERLACE, &len),
            leiaGetShader(LEIA_FRAGMENT_VIEW_INTERLACE, &len));
    view_sharpening_shader.program_ = leiaCreateProgram(
            leiaGetShader(LEIA_VERTEX_VIEW_SHARPENING, &len),
            leiaGetShader(LEIA_FRAGMENT_VIEW_SHARPENING, &len));
}

void MoreTeapotsRenderer::UpdateViewport() {
    // Init Projection matrices

    int32_t viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    view_width_pixels_ = 640;//LeiaJNIDisplayParameters::mViewResolution[0];
    view_height_pixels_ = 360;//LeiaJNIDisplayParameters::mViewResolution[1];
    screen_width_pixels_ = 2560;//LeiaJNIDisplayParameters::mScreenResolution[0];
    screen_height_pixels_ = 1440;//LeiaJNIDisplayParameters::mScreenResolution[1];
    float baseline_scaling = 1.0f;
    float to_radians = 3.14159f / 180.0f;
    float to_degrees = 180.0f / 3.14159f;
    // We convert to radians, make sure the aspect ratio is applied so things look the same
    // regardless of the orientation, then convert back to degrees
    float vertical_fov_degrees = atanf(tanf((66.9f * to_radians) / 2.0f) *
                                       (float) view_height_pixels_ /
                                       (float) fmax((float) view_width_pixels_,
                                                    (float) view_height_pixels_)) *
                                       to_degrees * 2.0f;
    float convergence_distance = 200.0f;
    leiaInitializeCameraData(&data, CAMERAS_WIDE, CAMERAS_HIGH,
                             8.0,//LeiaJNIDisplayParameters::mSystemDisparity,
                             baseline_scaling, convergence_distance,
                             vertical_fov_degrees, CAM_NEAR, CAM_FAR,
                             view_width_pixels_, view_height_pixels_);
    if (!leiaCalculateViews(&data, (LeiaCameraView *) cameras, CAMERAS_WIDE, CAMERAS_HIGH)) {
        LOGE("leiaCalculateViews did not work. The camera data is invalid.");
    }

    PrepareFullscreenSurface();
    PrepareRenderTargetSurfaces();

    const float CAM_NEAR = 5.f;
    const float CAM_FAR = 10000.f;
    if (viewport[2] < viewport[3]) {
        float aspect =
                static_cast<float>(viewport[2]) / static_cast<float>(viewport[3]);
        mat_projection_ =
                ndk_helper::Mat4::Perspective(aspect, 1.0f, CAM_NEAR, CAM_FAR);
    } else {
        float aspect =
                static_cast<float>(viewport[3]) / static_cast<float>(viewport[2]);
        mat_projection_ =
                ndk_helper::Mat4::Perspective(1.0f, aspect, CAM_NEAR, CAM_FAR);
    }
}

//--------------------------------------------------------------------------------
// Unload
//--------------------------------------------------------------------------------
void MoreTeapotsRenderer::Unload() {
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ibo_) {
        glDeleteBuffers(1, &ibo_);
        ibo_ = 0;
    }
    if (shader_param_.program_) {
        glDeleteProgram(shader_param_.program_);
        shader_param_.program_ = 0;
    }

    if (fullscreen_fbo) {
        glDeleteFramebuffers(1, &fullscreen_fbo);
        fullscreen_fbo = 0;
    }

    if (fullscreen_texture) {
        glDeleteFramebuffers(1, &fullscreen_texture);
        fullscreen_texture = 0;
    }

    if (fbos[0]) {
        glDeleteFramebuffers(RT_COUNT, fbos);
        glDeleteTextures(RT_COUNT, render_textures);
        glDeleteTextures(RT_COUNT, depth_textures);
        for (int i = 0; i < RT_COUNT; ++i) {
            fbos[i] = 0;
            render_textures[i] = 0;
            depth_textures[i] = 0;
        }
    }
    if (fbo_dof[RT_COUNT - 1]) {
        glDeleteTextures(RT_COUNT, fbo_dof);
        glDeleteTextures(RT_COUNT, texture_dof);
        for (int i = 0; i < RT_COUNT; ++i) {
            fbo_dof[i] = 0;
            texture_dof[i] = 0;
        }
    }

    if (checkerboard_texture) {
        glDeleteTextures(1, &checkerboard_texture);
        checkerboard_texture = 0;
    }

    if (dof_shader.program_) {
        glDeleteProgram(dof_shader.program_);
        dof_shader.program_ = 0;
    }
    if (view_interlacing_shader.program_) {
        glDeleteProgram(view_interlacing_shader.program_);
        view_interlacing_shader.program_ = 0;
    }
    if (view_sharpening_shader.program_) {
        glDeleteProgram(view_sharpening_shader.program_);
        view_sharpening_shader.program_ = 0;
    }
    if (texture_shader.program_) {
        glDeleteProgram(texture_shader.program_);
        texture_shader.program_ = 0;
    }
}

//--------------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------------
void MoreTeapotsRenderer::Update(float fTime, bool render_with_multiview_ext) {
    const float CAM_X = 0.f;
    const float CAM_Y = 0.f;
    const float CAM_Z = 800.f;

    mat_view_ = ndk_helper::Mat4::LookAt(ndk_helper::Vec3(CAM_X, CAM_Y, CAM_Z),
                                         ndk_helper::Vec3(0.f, 0.f, 0.f),
                                         ndk_helper::Vec3(0.f, 1.f, 0.f));

    if (camera_) {
        camera_->Update();
        mat_view_ = camera_->GetTransformMatrix() * mat_view_ *
                    camera_->GetRotationMatrix();
    }
}
//--------------------------------------------------------------------------------
// Render
//--------------------------------------------------------------------------------

void MoreTeapotsRenderer::RenderViews(bool is_backlight_still_on) {
    if (is_backlight_still_on) {

        static int count = 0;
        ++count;
        if (count > 50) {
            count = 0;
            using_simple_leia_rendering_api = !using_simple_leia_rendering_api;
        }
        float debug = 0.0f;
        glViewport(0, 0, view_width_pixels_, view_height_pixels_);
        for (unsigned int y = 0; y < CAMERAS_HIGH; ++y) {
            for (unsigned int x = 0; x < CAMERAS_WIDE; ++x) {
                unsigned int index = y * CAMERAS_WIDE + x;
                glBindFramebuffer(GL_FRAMEBUFFER, fbos[index]);
                glEnable(GL_DEPTH_TEST);
                glClearColor(0.4, 0.4, 0.4, 1.0);
                glClearDepthf(1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                RenderView(x, y, is_backlight_still_on);
                if (using_simple_leia_rendering_api) {
                    leiaDOF(render_textures[index], depth_textures[index],
                            &data, dof_shader.program_, fbo_dof[index], 1.0f);
                } else {
                    leiaPrepareDOF(render_textures[index], depth_textures[index],
                                   &data, dof_shader.program_, fbo_dof[index], 1.0f, debug);
                    leiaDrawQuad(dof_shader.program_, 0, 0);
                }
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        CHECK_GL_ERROR();

        if (using_simple_leia_rendering_api) {
            leiaViewInterlace(texture_dof, &data, view_interlacing_shader.program_,
                              fullscreen_fbo, screen_width_pixels_, screen_height_pixels_,
                              LeiaJNIDisplayParameters::mAlignmentOffset);
            leiaViewSharpening(fullscreen_texture, &data, view_sharpening_shader.program_, 0,
                               screen_width_pixels_,
                               LeiaJNIDisplayParameters::mViewSharpeningParams, 2);
        } else {
            leiaPrepareViewInterlace(texture_dof, &data, view_interlacing_shader.program_,
                                     fullscreen_fbo, screen_width_pixels_, screen_height_pixels_,
                                     LeiaJNIDisplayParameters::mAlignmentOffset, 0.0);
            leiaDrawQuad(view_interlacing_shader.program_, 0, 0);
            leiaPrepareViewSharpening(fullscreen_texture, &data, view_sharpening_shader.program_, 0,
                                      screen_width_pixels_,
                                      LeiaJNIDisplayParameters::mViewSharpeningParams, 2, debug);
            leiaDrawQuad(view_sharpening_shader.program_, 0, 0);
        }

    } else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.4, 0.4, 0.4, 1.0);
        glClearDepthf(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderView(-1, -1, is_backlight_still_on);
    }
}

void MoreTeapotsRenderer::RenderView(unsigned int x, unsigned int y, bool use_leia) {

    ndk_helper::Mat4 perspective;
    perspective = mat_projection_;
    if (use_leia) {
        for (int i = 0; i < 16; ++i) {
            perspective.Ptr()[i] = cameras[y][x].matrix[i];
        }
    }

    // Bind the VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    CHECK_GL_ERROR();
    int32_t iStride = sizeof(TEAPOT_VERTEX);
    // Pass the vertex data
    glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, GL_FALSE, iStride,
                          BUFFER_OFFSET(0));
    glEnableVertexAttribArray(ATTRIB_VERTEX);

    glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, iStride,
                          BUFFER_OFFSET(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(ATTRIB_NORMAL);

    // Bind the IB
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);

    glUseProgram(shader_param_.program_);

    TEAPOT_MATERIALS material = {{1.0f, 1.0f, 1.0f, 10.f},
                                 {0.1f, 0.1f, 0.1f},};

    // Update uniforms
    //
    // using glUniform3fv here was troublesome..
    //
    glUniform4f(shader_param_.material_specular_, material.specular_color[0],
                material.specular_color[1], material.specular_color[2],
                material.specular_color[3]);
    glUniform3f(shader_param_.material_ambient_, material.ambient_color[0],
                material.ambient_color[1], material.ambient_color[2]);

    glUniform3f(shader_param_.light0_, 100.f, -200.f, -600.f);

    // Regular rendering pass
    for (int32_t i = 0; i < teapot_x_ * teapot_y_ * teapot_z_; ++i) {
        // Set diffuse
        float x, y, z;
        vec_colors_[i].Value(x, y, z);
        glUniform4f(shader_param_.material_diffuse_, x, y, z, 1.f);

        // Rotation
        vec_current_rotations_[i] += vec_rotations_[i];
        vec_current_rotations_[i].Value(x, y);
        ndk_helper::Mat4 mat_rotation =
                ndk_helper::Mat4::RotationX(x) * ndk_helper::Mat4::RotationY(y);

        // Feed Projection and Model View matrices to the shaders
        ndk_helper::Mat4 mat_v = mat_view_ * vec_mat_models_[i] * mat_rotation;
        ndk_helper::Mat4 mat_vp = perspective * mat_v;
        glUniformMatrix4fv(shader_param_.matrix_projection_, 1, GL_FALSE,
                           mat_vp.Ptr());
        glUniformMatrix4fv(shader_param_.matrix_view_, 1, GL_FALSE, mat_v.Ptr());

        glDrawElements(GL_TRIANGLES, num_indices_, GL_UNSIGNED_SHORT,
                       BUFFER_OFFSET(0));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    CHECK_GL_ERROR();
}

//--------------------------------------------------------------------------------
// LoadShaders
//--------------------------------------------------------------------------------
bool MoreTeapotsRenderer::LoadShaders(SHADER_PARAMS *params, const char *strVsh,
                                      const char *strFsh) {
    //
    // Shader load for GLES2
    // In GLES2.0, shader attribute locations need to be explicitly specified
    // before linking
    //
    GLuint program;
    GLuint vertShader, fragShader;

    // Create shader program
    program = glCreateProgram();
    LOGI("Created Shader %d", program);

    // Create and compile vertex shader
    if (!ndk_helper::shader::CompileShader(&vertShader, GL_VERTEX_SHADER,
                                           strVsh)) {
        LOGI("Failed to compile vertex shader");
        glDeleteProgram(program);
        return false;
    }

    // Create and compile fragment shader
    if (!ndk_helper::shader::CompileShader(&fragShader, GL_FRAGMENT_SHADER,
                                           strFsh)) {
        LOGI("Failed to compile fragment shader");
        glDeleteProgram(program);
        return false;
    }

    // Attach vertex shader to program
    glAttachShader(program, vertShader);

    // Attach fragment shader to program
    glAttachShader(program, fragShader);

    // Bind attribute locations
    // this needs to be done prior to linking
    glBindAttribLocation(program, ATTRIB_VERTEX, "myVertex");
    glBindAttribLocation(program, ATTRIB_NORMAL, "myNormal");

    // Link program
    if (!ndk_helper::shader::LinkProgram(program)) {
        LOGI("Failed to link program: %d", program);

        if (vertShader) {
            glDeleteShader(vertShader);
            vertShader = 0;
        }
        if (fragShader) {
            glDeleteShader(fragShader);
            fragShader = 0;
        }
        if (program) {
            glDeleteProgram(program);
        }
        return false;
    }

    // Get uniform locations
    params->matrix_projection_ = glGetUniformLocation(program, "uPMatrix");
    params->matrix_view_ = glGetUniformLocation(program, "uMVMatrix");

    params->light0_ = glGetUniformLocation(program, "vLight0");
    params->material_diffuse_ = glGetUniformLocation(program, "vMaterialDiffuse");
    params->material_ambient_ = glGetUniformLocation(program, "vMaterialAmbient");
    params->material_specular_ =
            glGetUniformLocation(program, "vMaterialSpecular");

    // Release vertex and fragment shaders
    if (vertShader) glDeleteShader(vertShader);
    if (fragShader) glDeleteShader(fragShader);

    params->program_ = program;
    return true;
}

//--------------------------------------------------------------------------------
// Bind
//--------------------------------------------------------------------------------
bool MoreTeapotsRenderer::Bind(ndk_helper::TapCamera *camera) {
    camera_ = camera;
    return true;
}

void MoreTeapotsRenderer::PrepareFullscreenSurface() {
    glGenFramebuffers(1, &fullscreen_fbo);
    glGenTextures(1, &fullscreen_texture);
    glBindTexture(GL_TEXTURE_2D, fullscreen_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, screen_width_pixels_, screen_height_pixels_, 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fullscreen_fbo);
    GLenum attachment = GL_COLOR_ATTACHMENT0;
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, fullscreen_texture, 0);
    GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("FAILED ON %d FBO FOR Final FBO! FAILURE!", __LINE__);
    }
}

void MoreTeapotsRenderer::PrepareRenderTargetSurfaces() {
    glGenFramebuffers(RT_COUNT, fbos);
    glGenFramebuffers(RT_COUNT, fbo_dof);

    for (unsigned int i = 0; i < RT_COUNT; ++i) {
        // the objects need to be drawn into FBOs
        // This is the general rendering FBO set
        glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]);
        render_textures[i] = CreateTexture(view_width_pixels_, view_height_pixels_,
                                           GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        GLenum attachment = GL_COLOR_ATTACHMENT0;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
                               render_textures[i], 0);

        depth_textures[i] = CreateTexture(view_width_pixels_, view_height_pixels_,
                                          GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT,
                                          GL_FLOAT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, depth_textures[i], 0);

        glDrawBuffers(1, &attachment);
        GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOGE("FAILED ON %d FBO FOR INDIVIDUAL FBOS! FAILURE!", i);
        }

        // Create the FBOs and Textures needed for the DoF pass
        // Also verify the framebuffer is valid after creation
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_dof[i]);
        texture_dof[i] = CreateTexture(view_width_pixels_, view_height_pixels_, GL_RGBA8,
                                       GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        attachment = GL_COLOR_ATTACHMENT0;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture_dof[i], 0);
        glDrawBuffers(1, &attachment);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOGE("FAILED ON %d FBO FOR INDIVIDUAL FBOS! FAILURE!", i);
        }
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

GLuint MoreTeapotsRenderer::CreateTexture(int width, int height,
                                          GLint internal_format,
                                          GLenum format, GLenum type,
                                          void *data) {
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type,
                 data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texture_id;
}

void MoreTeapotsRenderer::DrawTexturedQuad(GLint program_id) {
    CHECK_GL_ERROR();

// vao and vbo handle
    static GLuint vbo, ibo;
    GLuint pos_loc = glGetAttribLocation(program_id, "myVertex");
    GLuint tex_loc = glGetAttribLocation(program_id, "myUV");

    // generate and bind the vertex buffer object
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    CHECK_GL_ERROR();

    // data for a fullscreen quad (this time with texture coords)
    GLfloat vertexData[] = {
            //  X     Y     Z           U     V
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f, // vertex 0
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // vertex 1
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f, // vertex 2
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, // vertex 3
    }; // 4 vertices with 5 components (floats) each

    // fill with data
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * 5, vertexData, GL_STATIC_DRAW);


    // set up generic attrib pointers
    glEnableVertexAttribArray(pos_loc);
    CHECK_GL_ERROR();
    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          (char *) 0 + 0 * sizeof(GLfloat));

    glEnableVertexAttribArray(tex_loc);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          (char *) 0 + 3 * sizeof(GLfloat));


    // generate and bind the index buffer object
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    CHECK_GL_ERROR();

    GLuint indexData[] = {
            0, 1, 2, // first triangle
            2, 1, 3, // second triangle
    };

    // fill with data
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * 2 * 3, indexData, GL_STATIC_DRAW);

    CHECK_GL_ERROR();

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          (char *) 0 + 0 * sizeof(GLfloat));
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          (char *) 0 + 3 * sizeof(GLfloat));
    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_loc);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    CHECK_GL_ERROR();

    // draw
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    CHECK_GL_ERROR();

    glDisableVertexAttribArray(pos_loc);
    glDisableVertexAttribArray(tex_loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
}
