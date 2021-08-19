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
// TeapotRenderer.cpp
// Render a teapot
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Include files
//--------------------------------------------------------------------------------
#include "TeapotRenderer.h"

//--------------------------------------------------------------------------------
// Teapot model data
//--------------------------------------------------------------------------------
#include "teapot.inl"
#include <cstdlib>

#include "LeiaCameraViews.h"
#include "LeiaNativeSDK.h"
#include "LeiaJNIDisplayParameters.h"

const unsigned int CAMERAS_HIGH = 1;
const unsigned int CAMERAS_WIDE = 4;
LeiaCameraView cameras[CAMERAS_HIGH][CAMERAS_WIDE];
LeiaCameraData data;

GLint leia_vbo;

//--------------------------------------------------------------------------------
// Ctor
//--------------------------------------------------------------------------------
TeapotRenderer::TeapotRenderer() {}

//--------------------------------------------------------------------------------
// Dtor
//--------------------------------------------------------------------------------
TeapotRenderer::~TeapotRenderer() { Unload(); }

void TeapotRenderer::Init() {
    using_simple_leia_rendering_api = false;
    // Settings
    glFrontFace(GL_CCW);

    // Load shader
    LoadShaders(&shader_param_, "Shaders/VS_ShaderPlain.vsh",
                "Shaders/ShaderPlain.fsh");
    LoadShaders(&texture_shader, "Shaders/VS_texture.vsh",
                "Shaders/texture.fsh");

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

    UpdateViewport();
    mat_model_ = ndk_helper::Mat4::Translation(0, 0, -15.f);

    ndk_helper::Mat4 mat = ndk_helper::Mat4::RotationX(M_PI / 3);
    mat_model_ = mat * mat_model_;

    unsigned int len = 0;
    dof_shader.program_ = leiaCreateProgram(leiaGetShader(LEIA_VERTEX_DOF, &len),
                                            leiaGetShader(LEIA_FRAGMENT_DOF, &len));
    view_interlacing_shader.program_ = leiaCreateProgram(
            leiaGetShader(LEIA_VERTEX_VIEW_INTERLACE, &len),
            leiaGetShader(LEIA_FRAGMENT_VIEW_INTERLACE, &len));
    view_sharpening_shader.program_ = leiaCreateProgram(
            leiaGetShader(LEIA_VERTEX_VIEW_SHARPENING, &len),
            leiaGetShader(LEIA_FRAGMENT_VIEW_SHARPENING, &len));
    leia_vbo = leiaBuildQuadVertexBuffer(view_sharpening_shader.program_);
}

void TeapotRenderer::UpdateViewport() {
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
                             8.0f,//LeiaJNIDisplayParameters::mSystemDisparity,
                             baseline_scaling, convergence_distance,
                             vertical_fov_degrees, CAM_NEAR, CAM_FAR,
                             view_width_pixels_, view_height_pixels_);
    if (!leiaCalculateViews(&data, (LeiaCameraView *) cameras, CAMERAS_WIDE, CAMERAS_HIGH)) {
        LOGE("leiaCalculateViews did not work. The camera data is invalid.");
    }

    PrepareFullscreenSurface();
    PrepareRenderTargetSurfaces();
    PrepareCheckerboard();
}

void TeapotRenderer::Unload() {
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

void TeapotRenderer::Update(float fTime) {
    const float CAM_X = 0.f;
    const float CAM_Y = 0.f;
    const float CAM_Z = 100.0f;
    ndk_helper::Mat4 mat_trans[3];
    ndk_helper::Mat4 mat_scale[3];

    mat_trans[0] = ndk_helper::Mat4::Translation(0, 0, 0);
    mat_trans[1] = ndk_helper::Mat4::Translation(-30.0, 45.0, -100.0);
    mat_trans[2] = ndk_helper::Mat4::Translation(50.0, -100.0, -300.0);

    mat_scale[0] = ndk_helper::Mat4::Identity();
    mat_scale[1] = ndk_helper::Mat4::Identity();
    mat_scale[2] = ndk_helper::Mat4::Identity();
    for (int i = 0; i < 3; ++i) {
        mat_view_[i] = ndk_helper::Mat4::LookAt(ndk_helper::Vec3(CAM_X, CAM_Y, CAM_Z),
                                                ndk_helper::Vec3(0.f, 0.f, 0.f),
                                                ndk_helper::Vec3(0.f, 1.f, 0.f));

        if (camera_) {
            camera_->Update();
            ndk_helper::Mat4 world =
                    mat_trans[i] * camera_->GetRotationMatrix() * mat_scale[i] * mat_model_;
            mat_view_[i] = camera_->GetTransformMatrix() * mat_view_[i] * world;
        } else {
            mat_view_[i] = mat_view_[i] * mat_trans[i] * mat_scale[i] * mat_model_;
        }
    }

}

void TeapotRenderer::RenderViews(bool is_backlight_still_on) {
    if (is_backlight_still_on) {

        static int count = 0;
        ++count;
        if (count > 50) {
            count = 0;
            using_simple_leia_rendering_api = !using_simple_leia_rendering_api;
        }

        static int vbo_id = 0;
        static int vbo_count = 0;
        ++vbo_count;
        if (vbo_count > 30) {
            vbo_count = 0;
            if (vbo_id) {
                leia_vbo = 0;
            }
            else {
                vbo_id = leia_vbo;
            }
        }

        float debug = 0.0f;
        glViewport(0, 0, view_width_pixels_, view_height_pixels_);
        for (unsigned int y = 0; y < CAMERAS_HIGH; ++y) {
            for (unsigned int x = 0; x < CAMERAS_WIDE; ++x) {
                unsigned int index = y * CAMERAS_WIDE + x;
                glBindFramebuffer(GL_FRAMEBUFFER, fbos[index]);
                glClearColor(1.0, 0.0, 1.0, 1.0);
                glEnable(GL_DEPTH_TEST);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                RenderView(x, y, is_backlight_still_on);
                if (using_simple_leia_rendering_api) {
                    leiaDOF(render_textures[index], depth_textures[index],
                            &data, dof_shader.program_, fbo_dof[index], 1.0f);
                } else {
                    leiaPrepareDOF(render_textures[index], depth_textures[index],
                                   &data, dof_shader.program_, fbo_dof[index], 1.0f, debug);
                    leiaDrawQuad(dof_shader.program_, 0, vbo_id);
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
            LOGE("simple");
        } else {
            leiaPrepareViewInterlace(texture_dof, &data, view_interlacing_shader.program_,
                                     fullscreen_fbo, screen_width_pixels_, screen_height_pixels_,
                                     LeiaJNIDisplayParameters::mAlignmentOffset, 0.0);
            leiaDrawQuad(view_interlacing_shader.program_, 0, vbo_id);
            leiaPrepareViewSharpening(fullscreen_texture, &data, view_sharpening_shader.program_, 0,
                                      screen_width_pixels_,
                                      LeiaJNIDisplayParameters::mViewSharpeningParams, 2, debug);
            leiaDrawQuad(view_sharpening_shader.program_, 0, vbo_id);
            LOGE("complex");
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

void TeapotRenderer::RenderView(unsigned int x, unsigned int y, bool use_leia) {

    ndk_helper::Mat4 perspective;
    perspective = mat_projection_;
    if (use_leia) {
        for (int i = 0; i < 16; ++i) {
            perspective.Ptr()[i] = cameras[y][x].matrix[i];
        }
    }

    ndk_helper::Mat4 mat_vp[3];
    for (int i = 0; i < 3; ++i) {
        mat_vp[i] = perspective * mat_view_[i];
    }

    glUseProgram(shader_param_.program_);
    // Bind the VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

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

    TEAPOT_MATERIALS material = {
            {1.0f, 0.5f, 0.5f},
            {1.0f, 1.0f, 1.0f, 10.f},
            {0.1f, 0.1f, 0.1f},};

    // Update uniforms
    glUniform4f(shader_param_.material_diffuse_, material.diffuse_color[0],
                material.diffuse_color[1], material.diffuse_color[2], 1.f);

    glUniform4f(shader_param_.material_specular_, material.specular_color[0],
                material.specular_color[1], material.specular_color[2],
                material.specular_color[3]);
    //
    // using glUniform3fv here was troublesome
    //
    glUniform3f(shader_param_.material_ambient_, material.ambient_color[0],
                material.ambient_color[1], material.ambient_color[2]);

    for (int i = 0; i < 3; ++i) {
        glUniformMatrix4fv(shader_param_.matrix_projection_, 1, GL_FALSE,
                           mat_vp[i].Ptr());
        glUniformMatrix4fv(shader_param_.matrix_view_, 1, GL_FALSE, mat_view_[i].Ptr());
        glUniform3f(shader_param_.light0_, 100.f, -200.f, -600.f);

        glDrawElements(GL_TRIANGLES, num_indices_, GL_UNSIGNED_SHORT,
                       BUFFER_OFFSET(0));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    ndk_helper::Mat4 persp_mat;
    persp_mat = perspective;
    DrawBillboard(persp_mat.Ptr());
}

bool TeapotRenderer::LoadShaders(SHADER_PARAMS *params, const char *strVsh,
                                 const char *strFsh) {
    GLuint program;
    GLuint vert_shader, frag_shader;

    // Create shader program
    program = glCreateProgram();
    LOGI("Created Shader %d", program);

    // Create and compile vertex shader
    if (!ndk_helper::shader::CompileShader(&vert_shader, GL_VERTEX_SHADER,
                                           strVsh)) {
        LOGI("Failed to compile vertex shader");
        glDeleteProgram(program);
        return false;
    }

    // Create and compile fragment shader
    if (!ndk_helper::shader::CompileShader(&frag_shader, GL_FRAGMENT_SHADER,
                                           strFsh)) {
        LOGI("Failed to compile fragment shader");
        glDeleteProgram(program);
        return false;
    }

    // Attach vertex shader to program
    glAttachShader(program, vert_shader);

    // Attach fragment shader to program
    glAttachShader(program, frag_shader);

    // Bind attribute locations
    // this needs to be done prior to linking
    glBindAttribLocation(program, ATTRIB_VERTEX, "myVertex");
    glBindAttribLocation(program, ATTRIB_NORMAL, "myNormal");
    glBindAttribLocation(program, ATTRIB_UV, "myUV");

    // Link program
    if (!ndk_helper::shader::LinkProgram(program)) {
        LOGI("Failed to link program: %d", program);

        if (vert_shader) {
            glDeleteShader(vert_shader);
            vert_shader = 0;
        }
        if (frag_shader) {
            glDeleteShader(frag_shader);
            frag_shader = 0;
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
    if (vert_shader) glDeleteShader(vert_shader);
    if (frag_shader) glDeleteShader(frag_shader);

    params->program_ = program;
    return true;
}

bool TeapotRenderer::Bind(ndk_helper::TapCamera *camera) {
    camera_ = camera;
    return true;
}

void TeapotRenderer::PrepareFullscreenSurface() {
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

void TeapotRenderer::PrepareRenderTargetSurfaces() {
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

void TeapotRenderer::PrepareCheckerboard() {
    // Making a texture that is the size of our 3d screen...because...
    unsigned char *ptex = new unsigned char[view_width_pixels_ *
                                            view_height_pixels_ *
                                            4];

    if (ptex) {
        // What colors will we have in our billboard checker pattern
        unsigned char colors[7][4] = {
                {255, 0,   0,   255},
                {0,   255, 0,   255},
                {0,   0,   255, 255},
                {255, 255, 0,   255},
                {255, 0,   255, 255},
                {0,   255, 255, 255},
                {255, 255, 255, 255}
        };
        enum {
            COLOR_RED,
            COLOR_GREEN,
            COLOR_BLUE,
            COLOR_YELLOW,
            COLOR_MAGENTA,
            COLOR_CYAN,
            COLOR_WHITE,
            NUMBER_OF_COLORS
        };

        // Each color will be 32 colored units wide
        int grid_size = 32;
        // We double that to jump over black squares
        int double_grid_size = grid_size * 2;

        // The first square - will it be colored?
        bool start_as_colored = true;
        // We are currently coloring in a square
        bool is_colored = true;
        // Index for the color we will start with
        // Ranges from
        int start_color = COLOR_RED;
        // The counter to change color in the x direction
        int count = 0;
        const int RESTART_COLOR_SQUARE = 0;
        for (int y = 0; y < view_height_pixels_; ++y) {
            // Every time we have finshed a solid square in the grid of a single color...
            if (y % grid_size == RESTART_COLOR_SQUARE) {
                start_as_colored = !start_as_colored;

                // This keeps us constantly moving through colors
                // We need start color to remember how a row of grid spaces started
                start_color = count;
                start_color += 1;
                if (start_color >= NUMBER_OF_COLORS) {
                    start_color = COLOR_RED;
                }
            }
            for (int x = 0; x < view_width_pixels_; ++x) {
                if (x == 0) {
                    // When we start a new line for the texture,
                    // We need to be sure to use the same starting color as the previous line
                    // This way we get solid colors in the texture grid.
                    is_colored = start_as_colored;
                    count = start_color;
                }
                // Have we finished coloring a full width of a square?
                if (x > 0 && x % grid_size == RESTART_COLOR_SQUARE) {
                    // Every time we have finished a black and colored square pair
                    // we need to change color
                    if (x % double_grid_size == 0) {
                        ++count;
                        if (count >= NUMBER_OF_COLORS) {
                            count = COLOR_RED;
                        }
                    }
                    // Every time we have covered "grid_size" points, its time to switch
                    // between colored and non-colored squares
                    is_colored = !is_colored;
                }
                if (is_colored) {
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 0] = colors[count][0];
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 1] = colors[count][1];
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 2] = colors[count][2];
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 3] = 255;
                } else {
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 0] = 0;
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 1] = 0;
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 2] = 0;
                    ptex[y * view_width_pixels_ * 4 + x * 4 + 3] = 255;
                }
            }
        }

        checkerboard_texture = CreateTexture(view_width_pixels_, view_height_pixels_,
                                             GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

        delete[] ptex;
    }
}

GLuint TeapotRenderer::CreateTexture(int width, int height,
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

void TeapotRenderer::DrawBillboard(float *persp) {

    CHECK_GL_ERROR();
    glUseProgram(texture_shader.program_);

    // Setup the billboard program sampler (Texture read)
    GLint tex_sampler_loc = glGetUniformLocation(texture_shader.program_, "tex_sampler");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, checkerboard_texture);
    glUniform1i(tex_sampler_loc, 0);

    // Effectively random location in the scene to move the quad to
    GLint translation_loc = glGetUniformLocation(texture_shader.program_, "translation");

    ndk_helper::Mat4 translation = ndk_helper::Mat4::Translation(0.0, 50.0, -200.0f);
    ndk_helper::Mat4 scalar = ndk_helper::Mat4::Scale(50.0, 50.0, 1.0);
    ndk_helper::Mat4 transform = translation * scalar;
    glUniformMatrix4fv(translation_loc, 1, GL_FALSE, transform.Ptr());

    const float CAM_X = 0.0f;
    const float CAM_Y = 0.0f;
    const float CAM_Z = 100.0f;
    ndk_helper::Mat4 cam = ndk_helper::Mat4::LookAt(ndk_helper::Vec3(CAM_X, CAM_Y, CAM_Z),
                                                    ndk_helper::Vec3(0.0f, 0.0f, 0.0f),
                                                    ndk_helper::Vec3(0.0f, 1.0f, 0.0f));

    cam = camera_->GetTransformMatrix() * cam;
    // We use the same perspective matrix as with the previous objects drawn
    GLint cam_loc = glGetUniformLocation(texture_shader.program_, "cam");
    glUniformMatrix4fv(cam_loc, 1, GL_FALSE, cam.Ptr());

    // We use the same perspective matrix as with the previous objects drawn
    GLint persp_loc = glGetUniformLocation(texture_shader.program_, "perspective");
    glUniformMatrix4fv(persp_loc, 1, GL_FALSE, persp);

    // Basic quad drawing
    DrawTexturedQuad(texture_shader.program_);
    CHECK_GL_ERROR();
}

void TeapotRenderer::DrawTexturedQuad(GLint program_id) {
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
