/* Shared OpenGL 3.3 Core / OpenGL ES 3.0 renderer backend implementation. */

#include "renderer_gl_core.h"
#include "renderer_gl_es.h"
#include "renderer_backend.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define APIENTRYP GL_APIENTRYP
#else
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef GLenum (APIENTRYP CoreGetErrorProc)(void);
typedef const GLubyte *(APIENTRYP CoreGetStringProc)(GLenum name);
typedef void (APIENTRYP CoreGetIntegervProc)(GLenum name, GLint *value);
typedef void (APIENTRYP CoreViewportProc)(GLint x, GLint y,
                                         GLsizei width, GLsizei height);
typedef void (APIENTRYP CoreClearColorProc)(GLfloat red, GLfloat green,
                                           GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP CoreClearProc)(GLbitfield mask);
typedef void (APIENTRYP CoreEnableProc)(GLenum capability);
typedef void (APIENTRYP CoreDisableProc)(GLenum capability);
typedef void (APIENTRYP CoreColorMaskProc)(GLboolean red, GLboolean green,
                                          GLboolean blue, GLboolean alpha);
typedef void (APIENTRYP CoreScissorProc)(GLint x, GLint y,
                                        GLsizei width, GLsizei height);
typedef void (APIENTRYP CorePolygonModeProc)(GLenum face, GLenum mode);
typedef void (APIENTRYP CoreBlendFuncProc)(GLenum source, GLenum destination);
typedef void (APIENTRYP CoreGenTexturesProc)(GLsizei count, GLuint *textures);
typedef void (APIENTRYP CoreBindTextureProc)(GLenum target, GLuint texture);
typedef void (APIENTRYP CoreTexParameteriProc)(GLenum target, GLenum name,
                                              GLint value);
typedef void (APIENTRYP CorePixelStoreiProc)(GLenum name, GLint value);
typedef void (APIENTRYP CoreTexImage2DProc)(GLenum target, GLint level,
                                           GLint internal_format,
                                           GLsizei width, GLsizei height,
                                           GLint border, GLenum format,
                                           GLenum type, const void *pixels);
typedef void (APIENTRYP CoreTexSubImage2DProc)(GLenum target, GLint level,
                                              GLint x, GLint y,
                                              GLsizei width, GLsizei height,
                                              GLenum format, GLenum type,
                                              const void *pixels);
typedef void (APIENTRYP CoreDeleteTexturesProc)(GLsizei count,
                                               const GLuint *textures);
typedef void (APIENTRYP CoreDrawArraysProc)(GLenum mode, GLint first,
                                           GLsizei count);

typedef struct CoreFunctions {
    CoreGetErrorProc get_error;
    CoreGetStringProc get_string;
    CoreGetIntegervProc get_integerv;
    CoreViewportProc viewport;
    CoreClearColorProc clear_color;
    CoreClearProc clear;
    CoreEnableProc enable;
    CoreDisableProc disable;
    CoreColorMaskProc color_mask;
    CoreScissorProc scissor;
#ifndef __EMSCRIPTEN__
    CorePolygonModeProc polygon_mode;
#endif
    PFNGLBLENDEQUATIONPROC blend_equation;
    CoreBlendFuncProc blend_func;
    CoreGenTexturesProc gen_textures;
    CoreBindTextureProc bind_texture;
    CoreTexParameteriProc tex_parameter_i;
    CorePixelStoreiProc pixel_store_i;
    CoreTexImage2DProc tex_image_2d;
    CoreTexSubImage2DProc tex_sub_image_2d;
    CoreDeleteTexturesProc delete_textures;
    PFNGLACTIVETEXTUREPROC active_texture;
    PFNGLGENVERTEXARRAYSPROC gen_vertex_arrays;
    PFNGLBINDVERTEXARRAYPROC bind_vertex_array;
    PFNGLDELETEVERTEXARRAYSPROC delete_vertex_arrays;
    PFNGLGENBUFFERSPROC gen_buffers;
    PFNGLBINDBUFFERPROC bind_buffer;
    PFNGLBUFFERDATAPROC buffer_data;
    PFNGLDELETEBUFFERSPROC delete_buffers;
    PFNGLCREATESHADERPROC create_shader;
    PFNGLSHADERSOURCEPROC shader_source;
    PFNGLCOMPILESHADERPROC compile_shader;
    PFNGLGETSHADERIVPROC get_shader_iv;
    PFNGLDELETESHADERPROC delete_shader;
    PFNGLCREATEPROGRAMPROC create_program;
    PFNGLATTACHSHADERPROC attach_shader;
    PFNGLBINDATTRIBLOCATIONPROC bind_attrib_location;
#ifndef __EMSCRIPTEN__
    PFNGLBINDFRAGDATALOCATIONPROC bind_frag_data_location;
#endif
    PFNGLLINKPROGRAMPROC link_program;
    PFNGLGETPROGRAMIVPROC get_program_iv;
    PFNGLDELETEPROGRAMPROC delete_program;
    PFNGLUSEPROGRAMPROC use_program;
    PFNGLGETUNIFORMLOCATIONPROC get_uniform_location;
    PFNGLUNIFORMMATRIX3FVPROC uniform_matrix_3fv;
    PFNGLUNIFORM2FPROC uniform_2f;
    PFNGLUNIFORM1IPROC uniform_1i;
    PFNGLENABLEVERTEXATTRIBARRAYPROC enable_vertex_attrib_array;
    PFNGLVERTEXATTRIBPOINTERPROC vertex_attrib_pointer;
    PFNGLBINDSAMPLERPROC bind_sampler;
    CoreDrawArraysProc draw_arrays;
} CoreFunctions;

typedef struct CoreContext {
    CoreFunctions gl;
    RendererGLProcLoader loader;
    void *loader_userdata;
    GLuint program;
    GLuint vertex_array;
    GLuint stream_buffer;
    GLuint white_texture;
    GLint transform_uniform;
    GLint framebuffer_size_uniform;
    GLint texture_uniform;
    int frame_width;
    int frame_height;
    int context_available;
    int is_es;
    unsigned int generation;
} CoreContext;

typedef struct CoreTexture {
    GLuint name;
    int width;
    int height;
    unsigned int generation;
} CoreTexture;

typedef struct CoreMesh {
    GLuint buffer;
    GLsizei vertex_count;
    unsigned int generation;
} CoreMesh;

enum {
    CORE_POSITION_ATTRIBUTE = 0,
    CORE_TEXCOORD_ATTRIBUTE = 1,
    CORE_COLOR_ATTRIBUTE = 2
};

static const char core_vertex_shader_source[] =
    "#version 330 core\n"
    "in vec2 a_position;\n"
    "in vec2 a_texcoord;\n"
    "in vec4 a_color;\n"
    "uniform mat3 u_transform;\n"
    "uniform vec2 u_framebuffer_size;\n"
    "out vec2 v_texcoord;\n"
    "out vec4 v_color;\n"
    "void main(void)\n"
    "{\n"
    "    vec2 position = (u_transform * vec3(a_position, 1.0)).xy;\n"
    "    vec2 clip_position = vec2(\n"
    "        position.x * 2.0 / u_framebuffer_size.x - 1.0,\n"
    "        1.0 - position.y * 2.0 / u_framebuffer_size.y);\n"
    "    gl_Position = vec4(clip_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "    v_color = a_color;\n"
    "}\n";

static const char core_fragment_shader_source[] =
    "#version 330 core\n"
    "uniform sampler2D u_texture;\n"
    "in vec2 v_texcoord;\n"
    "in vec4 v_color;\n"
    "out vec4 fragment_color;\n"
    "void main(void)\n"
    "{\n"
    "    fragment_color = texture(u_texture, v_texcoord) * v_color;\n"
    "}\n";

static const char es_vertex_shader_source[] =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 a_position;\n"
    "in vec2 a_texcoord;\n"
    "in vec4 a_color;\n"
    "uniform mat3 u_transform;\n"
    "uniform vec2 u_framebuffer_size;\n"
    "out vec2 v_texcoord;\n"
    "out vec4 v_color;\n"
    "void main(void)\n"
    "{\n"
    "    vec2 position = (u_transform * vec3(a_position, 1.0)).xy;\n"
    "    vec2 clip_position = vec2(\n"
    "        position.x * 2.0 / u_framebuffer_size.x - 1.0,\n"
    "        1.0 - position.y * 2.0 / u_framebuffer_size.y);\n"
    "    gl_Position = vec4(clip_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "    v_color = a_color;\n"
    "}\n";

static const char es_fragment_shader_source[] =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "in vec2 v_texcoord;\n"
    "in vec4 v_color;\n"
    "out vec4 fragment_color;\n"
    "void main(void)\n"
    "{\n"
    "    fragment_color = texture(u_texture, v_texcoord) * v_color;\n"
    "}\n";

static int Core_load_proc(RendererGLProcLoader loader, void *userdata,
                          const char *name, void *destination,
                          size_t destination_size)
{
    void *address = loader(userdata, name);

    if (address == NULL || destination_size != sizeof(address))
        return 0;
    memcpy(destination, &address, sizeof(address));
    return 1;
}

#define CORE_LOAD(functions, loader, userdata, member, symbol)                \
    Core_load_proc((loader), (userdata), (symbol),                            \
                   &(functions)->member, sizeof((functions)->member))

static int Core_load_functions(CoreFunctions *functions,
                               RendererGLProcLoader loader, void *userdata,
                               int is_es)
{
#ifdef __EMSCRIPTEN__
    (void)is_es;
#endif
    memset(functions, 0, sizeof(*functions));
    return CORE_LOAD(functions, loader, userdata, get_error, "glGetError")
        && CORE_LOAD(functions, loader, userdata, get_string, "glGetString")
        && CORE_LOAD(functions, loader, userdata, get_integerv,
                     "glGetIntegerv")
        && CORE_LOAD(functions, loader, userdata, viewport, "glViewport")
        && CORE_LOAD(functions, loader, userdata, clear_color, "glClearColor")
        && CORE_LOAD(functions, loader, userdata, clear, "glClear")
        && CORE_LOAD(functions, loader, userdata, enable, "glEnable")
        && CORE_LOAD(functions, loader, userdata, disable, "glDisable")
        && CORE_LOAD(functions, loader, userdata, color_mask, "glColorMask")
        && CORE_LOAD(functions, loader, userdata, scissor, "glScissor")
#ifndef __EMSCRIPTEN__
        && (is_es || CORE_LOAD(functions, loader, userdata, polygon_mode,
                              "glPolygonMode"))
#endif
        && CORE_LOAD(functions, loader, userdata, blend_equation,
                     "glBlendEquation")
        && CORE_LOAD(functions, loader, userdata, blend_func, "glBlendFunc")
        && CORE_LOAD(functions, loader, userdata, gen_textures,
                     "glGenTextures")
        && CORE_LOAD(functions, loader, userdata, bind_texture,
                     "glBindTexture")
        && CORE_LOAD(functions, loader, userdata, tex_parameter_i,
                     "glTexParameteri")
        && CORE_LOAD(functions, loader, userdata, pixel_store_i,
                     "glPixelStorei")
        && CORE_LOAD(functions, loader, userdata, tex_image_2d,
                     "glTexImage2D")
        && CORE_LOAD(functions, loader, userdata, tex_sub_image_2d,
                     "glTexSubImage2D")
        && CORE_LOAD(functions, loader, userdata, delete_textures,
                     "glDeleteTextures")
        && CORE_LOAD(functions, loader, userdata, active_texture,
                     "glActiveTexture")
        && CORE_LOAD(functions, loader, userdata, gen_vertex_arrays,
                     "glGenVertexArrays")
        && CORE_LOAD(functions, loader, userdata, bind_vertex_array,
                     "glBindVertexArray")
        && CORE_LOAD(functions, loader, userdata, delete_vertex_arrays,
                     "glDeleteVertexArrays")
        && CORE_LOAD(functions, loader, userdata, gen_buffers, "glGenBuffers")
        && CORE_LOAD(functions, loader, userdata, bind_buffer, "glBindBuffer")
        && CORE_LOAD(functions, loader, userdata, buffer_data, "glBufferData")
        && CORE_LOAD(functions, loader, userdata, delete_buffers,
                     "glDeleteBuffers")
        && CORE_LOAD(functions, loader, userdata, create_shader,
                     "glCreateShader")
        && CORE_LOAD(functions, loader, userdata, shader_source,
                     "glShaderSource")
        && CORE_LOAD(functions, loader, userdata, compile_shader,
                     "glCompileShader")
        && CORE_LOAD(functions, loader, userdata, get_shader_iv,
                     "glGetShaderiv")
        && CORE_LOAD(functions, loader, userdata, delete_shader,
                     "glDeleteShader")
        && CORE_LOAD(functions, loader, userdata, create_program,
                     "glCreateProgram")
        && CORE_LOAD(functions, loader, userdata, attach_shader,
                     "glAttachShader")
        && CORE_LOAD(functions, loader, userdata, bind_attrib_location,
                     "glBindAttribLocation")
#ifndef __EMSCRIPTEN__
        && (is_es || CORE_LOAD(functions, loader, userdata,
                              bind_frag_data_location,
                              "glBindFragDataLocation"))
#endif
        && CORE_LOAD(functions, loader, userdata, link_program,
                     "glLinkProgram")
        && CORE_LOAD(functions, loader, userdata, get_program_iv,
                     "glGetProgramiv")
        && CORE_LOAD(functions, loader, userdata, delete_program,
                     "glDeleteProgram")
        && CORE_LOAD(functions, loader, userdata, use_program, "glUseProgram")
        && CORE_LOAD(functions, loader, userdata, get_uniform_location,
                     "glGetUniformLocation")
        && CORE_LOAD(functions, loader, userdata, uniform_matrix_3fv,
                     "glUniformMatrix3fv")
        && CORE_LOAD(functions, loader, userdata, uniform_2f, "glUniform2f")
        && CORE_LOAD(functions, loader, userdata, uniform_1i, "glUniform1i")
        && CORE_LOAD(functions, loader, userdata, enable_vertex_attrib_array,
                     "glEnableVertexAttribArray")
        && CORE_LOAD(functions, loader, userdata, vertex_attrib_pointer,
                     "glVertexAttribPointer")
        && CORE_LOAD(functions, loader, userdata, bind_sampler,
                     "glBindSampler")
        && CORE_LOAD(functions, loader, userdata, draw_arrays,
                     "glDrawArrays");
}

#undef CORE_LOAD

static void Core_clear_errors(CoreContext *context)
{
    unsigned int error_count = 0;

    while (context->gl.get_error() != GL_NO_ERROR && error_count < 64)
        error_count++;
}

static int Core_operation_succeeded(CoreContext *context)
{
    GLenum error = context->gl.get_error();

    if (error == GL_NO_ERROR)
        return 1;
    Core_clear_errors(context);
    return 0;
}

static int Core_context_supported(CoreContext *context)
{
    GLint major = 0;
    GLint minor = 0;
#ifndef __EMSCRIPTEN__
    GLint profile = 0;
#endif
    const GLubyte *version;

    Core_clear_errors(context);
    version = context->gl.get_string(GL_VERSION);
    context->gl.get_integerv(GL_MAJOR_VERSION, &major);
    context->gl.get_integerv(GL_MINOR_VERSION, &minor);
    if (context->is_es) {
        return Core_operation_succeeded(context)
            && version != NULL
            && (strncmp((const char *)version, "OpenGL ES ", 10) == 0
                || strncmp((const char *)version, "WebGL 2.0", 9) == 0)
            && major >= 3;
    }
#ifndef __EMSCRIPTEN__
    context->gl.get_integerv(GL_CONTEXT_PROFILE_MASK, &profile);
    return Core_operation_succeeded(context)
        && version != NULL
        && (major > 3 || (major == 3 && minor >= 3))
        && (profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0;
#else
    return 0;
#endif
}

static int Core_vertex_data_valid(size_t vertex_count, GLsizeiptr *byte_count)
{
    if (vertex_count == 0 || vertex_count > (size_t)INT_MAX
        || vertex_count > (size_t)PTRDIFF_MAX / sizeof(RendererVertex2D)) {
        return 0;
    }
    *byte_count = (GLsizeiptr)(vertex_count * sizeof(RendererVertex2D));
    return 1;
}

static int Core_pixel_data_valid(int width, int height, size_t pitch)
{
    size_t row_bytes;

    if (width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / 4)
        return 0;
    row_bytes = (size_t)width * 4;
    if (pitch < row_bytes)
        return 0;
    return height == 1
        || pitch <= (SIZE_MAX - row_bytes) / (size_t)(height - 1);
}

static RendererStatus Core_upload_bound_texture(CoreContext *context,
                                                int x, int y,
                                                int width, int height,
                                                const uint8_t *pixels,
                                                size_t pitch)
{
    size_t row_bytes = (size_t)width * 4;
    int row;

    if (pitch == row_bytes) {
        context->gl.tex_sub_image_2d(GL_TEXTURE_2D, 0, x, y, width, height,
                                     GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    } else {
        for (row = 0; row < height; row++) {
            context->gl.tex_sub_image_2d(
                GL_TEXTURE_2D, 0, x, y + row, width, 1,
                GL_RGBA, GL_UNSIGNED_BYTE,
                pixels + (size_t)row * pitch);
        }
    }
    return Core_operation_succeeded(context) ? RENDERER_STATUS_OK
                                             : RENDERER_STATUS_BACKEND_ERROR;
}

static RendererStatus Core_configure_and_upload_texture(
    CoreContext *context, GLuint texture, const RendererTextureDesc *desc,
    const uint8_t *pixels, size_t pitch)
{
    GLint previous_active_texture;
    GLint previous_texture;
    GLint previous_unpack_alignment;
    GLint previous_unpack_buffer;
    GLint previous_unpack_row_length;
    GLint previous_unpack_skip_rows;
    GLint previous_unpack_skip_pixels;
    GLint filter;
    GLint wrap;
    RendererStatus status = RENDERER_STATUS_BACKEND_ERROR;

    filter = desc->filter == RENDERER_TEXTURE_FILTER_LINEAR
        ? GL_LINEAR : GL_NEAREST;
    wrap = desc->wrap == RENDERER_TEXTURE_WRAP_REPEAT
        ? GL_REPEAT : GL_CLAMP_TO_EDGE;

    Core_clear_errors(context);
    context->gl.get_integerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    context->gl.active_texture(GL_TEXTURE0);
    context->gl.get_integerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    context->gl.get_integerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
    context->gl.get_integerv(GL_PIXEL_UNPACK_BUFFER_BINDING,
                             &previous_unpack_buffer);
    context->gl.get_integerv(GL_UNPACK_ROW_LENGTH,
                             &previous_unpack_row_length);
    context->gl.get_integerv(GL_UNPACK_SKIP_ROWS,
                             &previous_unpack_skip_rows);
    context->gl.get_integerv(GL_UNPACK_SKIP_PIXELS,
                             &previous_unpack_skip_pixels);
    if (!Core_operation_succeeded(context))
        return RENDERER_STATUS_BACKEND_ERROR;

    Core_clear_errors(context);
    context->gl.bind_texture(GL_TEXTURE_2D, texture);
    context->gl.bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0);
    context->gl.pixel_store_i(GL_UNPACK_ALIGNMENT, 1);
    context->gl.pixel_store_i(GL_UNPACK_ROW_LENGTH, 0);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_ROWS, 0);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_PIXELS, 0);
    context->gl.tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                filter);
    context->gl.tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                filter);
    context->gl.tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                wrap);
    context->gl.tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                wrap);
    context->gl.tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA8,
                             desc->width, desc->height, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    if (Core_operation_succeeded(context)) {
        Core_clear_errors(context);
        status = Core_upload_bound_texture(context, 0, 0,
                                           desc->width, desc->height,
                                           pixels, pitch);
    }

    Core_clear_errors(context);
    context->gl.bind_buffer(GL_PIXEL_UNPACK_BUFFER,
                            (GLuint)previous_unpack_buffer);
    context->gl.pixel_store_i(GL_UNPACK_ALIGNMENT,
                              previous_unpack_alignment);
    context->gl.pixel_store_i(GL_UNPACK_ROW_LENGTH,
                              previous_unpack_row_length);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_ROWS,
                              previous_unpack_skip_rows);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_PIXELS,
                              previous_unpack_skip_pixels);
    context->gl.bind_texture(GL_TEXTURE_2D, (GLuint)previous_texture);
    context->gl.active_texture((GLenum)previous_active_texture);
    if (!Core_operation_succeeded(context))
        return RENDERER_STATUS_BACKEND_ERROR;
    return status;
}

static GLuint Core_compile_shader(CoreContext *context, GLenum type,
                                  const char *source)
{
    const GLchar *shader_source = (const GLchar *)source;
    GLint compiled = GL_FALSE;
    GLuint shader;

    Core_clear_errors(context);
    shader = context->gl.create_shader(type);
    if (shader == 0 || !Core_operation_succeeded(context))
        return 0;
    context->gl.shader_source(shader, 1, &shader_source, NULL);
    context->gl.compile_shader(shader);
    context->gl.get_shader_iv(shader, GL_COMPILE_STATUS, &compiled);
    if (!Core_operation_succeeded(context) || compiled != GL_TRUE) {
        context->gl.delete_shader(shader);
        Core_clear_errors(context);
        return 0;
    }
    return shader;
}

static int Core_create_program(CoreContext *context)
{
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLint linked = GL_FALSE;

    vertex_shader = Core_compile_shader(
        context, GL_VERTEX_SHADER,
        context->is_es ? es_vertex_shader_source : core_vertex_shader_source);
    if (vertex_shader == 0)
        return 0;
    fragment_shader = Core_compile_shader(
        context, GL_FRAGMENT_SHADER,
        context->is_es ? es_fragment_shader_source
                       : core_fragment_shader_source);
    if (fragment_shader == 0) {
        context->gl.delete_shader(vertex_shader);
        Core_clear_errors(context);
        return 0;
    }

    Core_clear_errors(context);
    context->program = context->gl.create_program();
    if (context->program != 0) {
        context->gl.attach_shader(context->program, vertex_shader);
        context->gl.attach_shader(context->program, fragment_shader);
        context->gl.bind_attrib_location(context->program,
                                         CORE_POSITION_ATTRIBUTE,
                                         "a_position");
        context->gl.bind_attrib_location(context->program,
                                         CORE_TEXCOORD_ATTRIBUTE,
                                         "a_texcoord");
        context->gl.bind_attrib_location(context->program,
                                         CORE_COLOR_ATTRIBUTE,
                                         "a_color");
#ifndef __EMSCRIPTEN__
        if (!context->is_es) {
            context->gl.bind_frag_data_location(context->program, 0,
                                                "fragment_color");
        }
#endif
        context->gl.link_program(context->program);
        context->gl.get_program_iv(context->program, GL_LINK_STATUS, &linked);
    }
    context->gl.delete_shader(fragment_shader);
    context->gl.delete_shader(vertex_shader);
    if (context->program == 0 || linked != GL_TRUE
        || !Core_operation_succeeded(context)) {
        if (context->program != 0)
            context->gl.delete_program(context->program);
        context->program = 0;
        Core_clear_errors(context);
        return 0;
    }

    context->transform_uniform = context->gl.get_uniform_location(
        context->program, "u_transform");
    context->framebuffer_size_uniform = context->gl.get_uniform_location(
        context->program, "u_framebuffer_size");
    context->texture_uniform = context->gl.get_uniform_location(
        context->program, "u_texture");
    if (context->transform_uniform < 0
        || context->framebuffer_size_uniform < 0
        || context->texture_uniform < 0
        || !Core_operation_succeeded(context)) {
        context->gl.delete_program(context->program);
        context->program = 0;
        Core_clear_errors(context);
        return 0;
    }
    return 1;
}

static void Core_release_context_objects(CoreContext *context)
{
    GLint current_program = 0;

    if (context->white_texture != 0)
        context->gl.delete_textures(1, &context->white_texture);
    if (context->stream_buffer != 0)
        context->gl.delete_buffers(1, &context->stream_buffer);
    if (context->vertex_array != 0)
        context->gl.delete_vertex_arrays(1, &context->vertex_array);
    if (context->program != 0) {
        Core_clear_errors(context);
        context->gl.get_integerv(GL_CURRENT_PROGRAM, &current_program);
        if (Core_operation_succeeded(context)
            && (GLuint)current_program == context->program) {
            context->gl.use_program(0);
        }
        context->gl.delete_program(context->program);
    }
    context->white_texture = 0;
    context->stream_buffer = 0;
    context->vertex_array = 0;
    context->program = 0;
    Core_clear_errors(context);
}

static RendererStatus Core_create_context_objects(CoreContext *context)
{
    static const uint8_t white_pixel[] = {255, 255, 255, 255};
    static const RendererTextureDesc white_texture_desc = {
        1, 1,
        RENDERER_TEXTURE_FILTER_NEAREST,
        RENDERER_TEXTURE_WRAP_CLAMP
    };
    RendererStatus status;

    if (!Core_create_program(context))
        return RENDERER_STATUS_BACKEND_ERROR;

    Core_clear_errors(context);
    context->gl.gen_vertex_arrays(1, &context->vertex_array);
    context->gl.gen_buffers(1, &context->stream_buffer);
    context->gl.gen_textures(1, &context->white_texture);
    if (context->vertex_array == 0 || context->stream_buffer == 0
        || context->white_texture == 0
        || !Core_operation_succeeded(context)) {
        Core_release_context_objects(context);
        return RENDERER_STATUS_BACKEND_ERROR;
    }
    status = Core_configure_and_upload_texture(
        context, context->white_texture, &white_texture_desc, white_pixel,
        sizeof(white_pixel));
    if (status != RENDERER_STATUS_OK)
        Core_release_context_objects(context);
    return status;
}

static void Core_prepare_raster_state(CoreContext *context)
{
    context->gl.disable(GL_RASTERIZER_DISCARD);
    context->gl.disable(GL_CULL_FACE);
    context->gl.disable(GL_DEPTH_TEST);
    context->gl.disable(GL_STENCIL_TEST);
    context->gl.disable(GL_DITHER);
    context->gl.disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    context->gl.disable(GL_SAMPLE_COVERAGE);
#ifndef __EMSCRIPTEN__
    if (!context->is_es) {
        context->gl.disable(GL_COLOR_LOGIC_OP);
        context->gl.disable(GL_FRAMEBUFFER_SRGB);
        context->gl.disable(GL_POLYGON_SMOOTH);
        context->gl.disable(GL_SAMPLE_ALPHA_TO_ONE);
        context->gl.disable(GL_SAMPLE_MASK);
        context->gl.polygon_mode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif
    context->gl.color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

static RendererStatus Core_begin_frame(void *opaque, int width, int height,
                                       RendererColor clear_color)
{
    CoreContext *context = opaque;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    context->frame_width = width;
    context->frame_height = height;
    context->gl.viewport(0, 0, width, height);
    context->gl.disable(GL_SCISSOR_TEST);
    context->gl.disable(GL_BLEND);
    Core_prepare_raster_state(context);
    context->gl.clear_color((GLfloat)clear_color.red / 255.0f,
                            (GLfloat)clear_color.green / 255.0f,
                            (GLfloat)clear_color.blue / 255.0f,
                            (GLfloat)clear_color.alpha / 255.0f);
    context->gl.clear(GL_COLOR_BUFFER_BIT);
    return RENDERER_STATUS_OK;
}

static RendererStatus Core_set_draw_state(CoreContext *context,
                                          const RendererBackendDraw *draw)
{
    int64_t scissor_bottom;

    if (draw->blend == RENDERER_BLEND_OPAQUE) {
        context->gl.disable(GL_BLEND);
    } else {
        context->gl.enable(GL_BLEND);
        context->gl.blend_equation(GL_FUNC_ADD);
        if (draw->blend == RENDERER_BLEND_ALPHA) {
            context->gl.blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else if (draw->blend == RENDERER_BLEND_ADDITIVE) {
            context->gl.blend_func(GL_SRC_ALPHA, GL_ONE);
        } else {
            return RENDERER_STATUS_INVALID_ARGUMENT;
        }
    }

    if (draw->scissor_enabled) {
        if (draw->scissor.x < 0 || draw->scissor.y < 0
            || draw->scissor.width <= 0 || draw->scissor.height <= 0
            || draw->scissor.x > context->frame_width - draw->scissor.width
            || draw->scissor.y > context->frame_height - draw->scissor.height) {
            return RENDERER_STATUS_INVALID_ARGUMENT;
        }
        scissor_bottom = (int64_t)context->frame_height
            - draw->scissor.y - draw->scissor.height;
        context->gl.enable(GL_SCISSOR_TEST);
        context->gl.scissor(draw->scissor.x, (GLint)scissor_bottom,
                            draw->scissor.width, draw->scissor.height);
    } else {
        context->gl.disable(GL_SCISSOR_TEST);
    }
    return RENDERER_STATUS_OK;
}

static void Core_bind_vertex_layout(CoreContext *context, GLuint buffer)
{
    context->gl.bind_buffer(GL_ARRAY_BUFFER, buffer);
    context->gl.enable_vertex_attrib_array(CORE_POSITION_ATTRIBUTE);
    context->gl.vertex_attrib_pointer(
        CORE_POSITION_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(RendererVertex2D),
        (const void *)offsetof(RendererVertex2D, x));
    context->gl.enable_vertex_attrib_array(CORE_TEXCOORD_ATTRIBUTE);
    context->gl.vertex_attrib_pointer(
        CORE_TEXCOORD_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(RendererVertex2D),
        (const void *)offsetof(RendererVertex2D, u));
    context->gl.enable_vertex_attrib_array(CORE_COLOR_ATTRIBUTE);
    context->gl.vertex_attrib_pointer(
        CORE_COLOR_ATTRIBUTE, 4, GL_UNSIGNED_BYTE, GL_TRUE,
        (GLsizei)sizeof(RendererVertex2D),
        (const void *)offsetof(RendererVertex2D, color));
}

static RendererStatus Core_draw(void *opaque,
                                const RendererBackendDraw *draw)
{
    CoreContext *context = opaque;
    CoreTexture *texture;
    CoreMesh *mesh;
    GLsizeiptr byte_count;
    GLuint buffer;
    GLsizei vertex_count;
    RendererStatus status;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    if (draw == NULL || context->frame_width <= 0 || context->frame_height <= 0)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    if (draw->mesh != NULL) {
        if (draw->vertices != NULL || draw->vertex_count != 0)
            return RENDERER_STATUS_INVALID_ARGUMENT;
        mesh = draw->mesh;
        if (mesh->generation != context->generation)
            return RENDERER_STATUS_BACKEND_ERROR;
        buffer = mesh->buffer;
        vertex_count = mesh->vertex_count;
    } else {
        if (draw->vertices == NULL
            || !Core_vertex_data_valid(draw->vertex_count, &byte_count)) {
            return RENDERER_STATUS_INVALID_ARGUMENT;
        }
        buffer = context->stream_buffer;
        vertex_count = (GLsizei)draw->vertex_count;
    }

    status = Core_set_draw_state(context, draw);
    if (status != RENDERER_STATUS_OK)
        return status;
    context->gl.use_program(context->program);
    context->gl.bind_vertex_array(context->vertex_array);
    context->gl.uniform_matrix_3fv(context->transform_uniform, 1, GL_FALSE,
                                  draw->transform.m);
    context->gl.uniform_2f(context->framebuffer_size_uniform,
                          (GLfloat)context->frame_width,
                          (GLfloat)context->frame_height);
    context->gl.uniform_1i(context->texture_uniform, 0);
    context->gl.active_texture(GL_TEXTURE0);
    context->gl.bind_sampler(0, 0);
    texture = draw->texture;
    if (texture != NULL && texture->generation != context->generation)
        return RENDERER_STATUS_BACKEND_ERROR;
    context->gl.bind_texture(GL_TEXTURE_2D,
                             texture != NULL ? texture->name
                                             : context->white_texture);
    Core_bind_vertex_layout(context, buffer);
    if (draw->mesh == NULL) {
        context->gl.buffer_data(GL_ARRAY_BUFFER, byte_count, draw->vertices,
                                GL_STREAM_DRAW);
    }
    context->gl.draw_arrays(GL_TRIANGLES, 0, vertex_count);
    return RENDERER_STATUS_OK;
}

static RendererStatus Core_flush(void *opaque)
{
    CoreContext *context = opaque;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    /* GL preserves command order without an explicit flush. Presentation by
     * SDL submits the completed frame to the implementation. */
    return RENDERER_STATUS_OK;
}

static RendererStatus Core_end_frame(void *opaque)
{
    CoreContext *context = opaque;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    context->frame_width = 0;
    context->frame_height = 0;
    return RENDERER_STATUS_OK;
}

static RendererStatus Core_create_texture_name(
    CoreContext *context, const RendererTextureDesc *desc,
    const uint8_t *rgba_pixels, size_t pitch, GLuint *name)
{
    RendererStatus status;

    *name = 0;
    Core_clear_errors(context);
    context->gl.gen_textures(1, name);
    if (*name == 0 || !Core_operation_succeeded(context))
        return RENDERER_STATUS_BACKEND_ERROR;
    status = Core_configure_and_upload_texture(context, *name, desc,
                                               rgba_pixels, pitch);
    if (status != RENDERER_STATUS_OK) {
        context->gl.delete_textures(1, name);
        *name = 0;
        Core_clear_errors(context);
    }
    return status;
}

static RendererStatus Core_texture_create(void *opaque,
                                          const RendererTextureDesc *desc,
                                          const uint8_t *rgba_pixels,
                                          size_t pitch, void **handle)
{
    CoreContext *context = opaque;
    CoreTexture *texture;
    RendererStatus status;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    if (handle == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    *handle = NULL;
    if (desc == NULL || rgba_pixels == NULL
        || !Core_pixel_data_valid(desc->width, desc->height, pitch)) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    texture = calloc(1, sizeof(*texture));
    if (texture == NULL)
        return RENDERER_STATUS_OUT_OF_MEMORY;

    status = Core_create_texture_name(context, desc, rgba_pixels, pitch,
                                      &texture->name);
    if (status != RENDERER_STATUS_OK) {
        free(texture);
        return status;
    }
    texture->width = desc->width;
    texture->height = desc->height;
    texture->generation = context->generation;
    *handle = texture;
    return RENDERER_STATUS_OK;
}

static RendererStatus Core_texture_update(void *opaque, void *handle,
                                          RendererRect region,
                                          const uint8_t *rgba_pixels,
                                          size_t pitch)
{
    CoreContext *context = opaque;
    CoreTexture *texture = handle;
    GLint previous_active_texture;
    GLint previous_texture;
    GLint previous_unpack_alignment;
    GLint previous_unpack_buffer;
    GLint previous_unpack_row_length;
    GLint previous_unpack_skip_rows;
    GLint previous_unpack_skip_pixels;
    RendererStatus status;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    if (texture == NULL || texture->generation != context->generation
        || rgba_pixels == NULL
        || region.x < 0 || region.y < 0
        || region.width <= 0 || region.height <= 0
        || region.x > texture->width - region.width
        || region.y > texture->height - region.height
        || !Core_pixel_data_valid(region.width, region.height, pitch)) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    Core_clear_errors(context);
    context->gl.get_integerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    context->gl.active_texture(GL_TEXTURE0);
    context->gl.get_integerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    context->gl.get_integerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
    context->gl.get_integerv(GL_PIXEL_UNPACK_BUFFER_BINDING,
                             &previous_unpack_buffer);
    context->gl.get_integerv(GL_UNPACK_ROW_LENGTH,
                             &previous_unpack_row_length);
    context->gl.get_integerv(GL_UNPACK_SKIP_ROWS,
                             &previous_unpack_skip_rows);
    context->gl.get_integerv(GL_UNPACK_SKIP_PIXELS,
                             &previous_unpack_skip_pixels);
    if (!Core_operation_succeeded(context))
        return RENDERER_STATUS_BACKEND_ERROR;

    Core_clear_errors(context);
    context->gl.bind_texture(GL_TEXTURE_2D, texture->name);
    context->gl.bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0);
    context->gl.pixel_store_i(GL_UNPACK_ALIGNMENT, 1);
    context->gl.pixel_store_i(GL_UNPACK_ROW_LENGTH, 0);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_ROWS, 0);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_PIXELS, 0);
    status = Core_upload_bound_texture(context, region.x, region.y,
                                       region.width, region.height,
                                       rgba_pixels, pitch);

    Core_clear_errors(context);
    context->gl.bind_buffer(GL_PIXEL_UNPACK_BUFFER,
                            (GLuint)previous_unpack_buffer);
    context->gl.pixel_store_i(GL_UNPACK_ALIGNMENT,
                              previous_unpack_alignment);
    context->gl.pixel_store_i(GL_UNPACK_ROW_LENGTH,
                              previous_unpack_row_length);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_ROWS,
                              previous_unpack_skip_rows);
    context->gl.pixel_store_i(GL_UNPACK_SKIP_PIXELS,
                              previous_unpack_skip_pixels);
    context->gl.bind_texture(GL_TEXTURE_2D, (GLuint)previous_texture);
    context->gl.active_texture((GLenum)previous_active_texture);
    if (!Core_operation_succeeded(context))
        return RENDERER_STATUS_BACKEND_ERROR;
    return status;
}

static void Core_texture_destroy(void *opaque, void *handle)
{
    CoreContext *context = opaque;
    CoreTexture *texture = handle;

    if (texture == NULL)
        return;
    if (context->context_available
        && texture->generation == context->generation
        && texture->name != 0) {
        context->gl.delete_textures(1, &texture->name);
        Core_clear_errors(context);
    }
    free(texture);
}

static RendererStatus Core_create_mesh_buffer(
    CoreContext *context, const RendererVertex2D *vertices,
    size_t vertex_count, GLuint *buffer)
{
    GLsizeiptr byte_count;
    GLint previous_buffer;

    *buffer = 0;
    if (!Core_vertex_data_valid(vertex_count, &byte_count))
        return RENDERER_STATUS_INVALID_ARGUMENT;
    Core_clear_errors(context);
    context->gl.get_integerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
    context->gl.gen_buffers(1, buffer);
    context->gl.bind_buffer(GL_ARRAY_BUFFER, *buffer);
    context->gl.buffer_data(GL_ARRAY_BUFFER, byte_count, vertices,
                            GL_STATIC_DRAW);
    context->gl.bind_buffer(GL_ARRAY_BUFFER, (GLuint)previous_buffer);
    if (*buffer == 0 || !Core_operation_succeeded(context)) {
        if (*buffer != 0)
            context->gl.delete_buffers(1, buffer);
        *buffer = 0;
        Core_clear_errors(context);
        return RENDERER_STATUS_BACKEND_ERROR;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Core_mesh_create(void *opaque,
                                       const RendererVertex2D *vertices,
                                       size_t vertex_count, void **handle)
{
    CoreContext *context = opaque;
    CoreMesh *mesh;
    RendererStatus status;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    if (handle == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    *handle = NULL;
    if (vertices == NULL || vertex_count == 0) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    mesh = calloc(1, sizeof(*mesh));
    if (mesh == NULL)
        return RENDERER_STATUS_OUT_OF_MEMORY;

    status = Core_create_mesh_buffer(context, vertices, vertex_count,
                                     &mesh->buffer);
    if (status != RENDERER_STATUS_OK) {
        free(mesh);
        return status;
    }
    mesh->vertex_count = (GLsizei)vertex_count;
    mesh->generation = context->generation;
    *handle = mesh;
    return RENDERER_STATUS_OK;
}

static RendererStatus Core_mesh_update(void *opaque, void *handle,
                                       const RendererVertex2D *vertices,
                                       size_t vertex_count)
{
    CoreContext *context = opaque;
    CoreMesh *mesh = handle;
    GLsizeiptr byte_count;
    GLint previous_buffer;
    GLuint replacement = 0;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    if (mesh == NULL || mesh->generation != context->generation
        || vertices == NULL
        || !Core_vertex_data_valid(vertex_count, &byte_count)) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    Core_clear_errors(context);
    context->gl.get_integerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
    context->gl.gen_buffers(1, &replacement);
    context->gl.bind_buffer(GL_ARRAY_BUFFER, replacement);
    context->gl.buffer_data(GL_ARRAY_BUFFER, byte_count, vertices,
                            GL_STATIC_DRAW);
    context->gl.bind_buffer(GL_ARRAY_BUFFER, (GLuint)previous_buffer);
    if (replacement == 0 || !Core_operation_succeeded(context)) {
        if (replacement != 0)
            context->gl.delete_buffers(1, &replacement);
        Core_clear_errors(context);
        return RENDERER_STATUS_BACKEND_ERROR;
    }

    context->gl.delete_buffers(1, &mesh->buffer);
    mesh->buffer = replacement;
    mesh->vertex_count = (GLsizei)vertex_count;
    return Core_operation_succeeded(context) ? RENDERER_STATUS_OK
                                             : RENDERER_STATUS_BACKEND_ERROR;
}

static void Core_mesh_destroy(void *opaque, void *handle)
{
    CoreContext *context = opaque;
    CoreMesh *mesh = handle;

    if (mesh == NULL)
        return;
    if (context->context_available
        && mesh->generation == context->generation
        && mesh->buffer != 0) {
        context->gl.delete_buffers(1, &mesh->buffer);
        Core_clear_errors(context);
    }
    free(mesh);
}

static void Core_context_lost(void *opaque)
{
    CoreContext *context = opaque;

    if (!context->context_available)
        return;
    context->program = 0;
    context->vertex_array = 0;
    context->stream_buffer = 0;
    context->white_texture = 0;
    context->frame_width = 0;
    context->frame_height = 0;
    context->context_available = 0;
    context->generation++;
    if (context->generation == 0)
        context->generation = 1;
}

static RendererStatus Core_context_restore(void *opaque)
{
    CoreContext *context = opaque;
    RendererStatus status;

    if (context->context_available) {
        Core_release_context_objects(context);
        context->context_available = 0;
    }
    context->frame_width = 0;
    context->frame_height = 0;
    if (!Core_load_functions(&context->gl, context->loader,
                             context->loader_userdata, context->is_es)
        || !Core_context_supported(context)) {
        return RENDERER_STATUS_BACKEND_ERROR;
    }
    status = Core_create_context_objects(context);
    if (status == RENDERER_STATUS_OK)
        context->context_available = 1;
    return status;
}

static RendererStatus Core_texture_restore(
    void *opaque, void *handle, const RendererTextureDesc *desc,
    const uint8_t *rgba_pixels, size_t pitch)
{
    CoreContext *context = opaque;
    CoreTexture *texture = handle;
    GLuint replacement;
    RendererStatus status;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    if (texture == NULL || desc == NULL || rgba_pixels == NULL
        || !Core_pixel_data_valid(desc->width, desc->height, pitch)) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    status = Core_create_texture_name(context, desc, rgba_pixels, pitch,
                                      &replacement);
    if (status != RENDERER_STATUS_OK)
        return status;
    if (texture->generation == context->generation && texture->name != 0)
        context->gl.delete_textures(1, &texture->name);
    texture->name = replacement;
    texture->width = desc->width;
    texture->height = desc->height;
    texture->generation = context->generation;
    return Core_operation_succeeded(context) ? RENDERER_STATUS_OK
                                             : RENDERER_STATUS_BACKEND_ERROR;
}

static RendererStatus Core_mesh_restore(void *opaque, void *handle,
                                        const RendererVertex2D *vertices,
                                        size_t vertex_count)
{
    CoreContext *context = opaque;
    CoreMesh *mesh = handle;
    GLuint replacement;
    RendererStatus status;

    if (!context->context_available)
        return RENDERER_STATUS_INVALID_STATE;
    if (mesh == NULL || vertices == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    status = Core_create_mesh_buffer(context, vertices, vertex_count,
                                     &replacement);
    if (status != RENDERER_STATUS_OK)
        return status;
    if (mesh->generation == context->generation && mesh->buffer != 0)
        context->gl.delete_buffers(1, &mesh->buffer);
    mesh->buffer = replacement;
    mesh->vertex_count = (GLsizei)vertex_count;
    mesh->generation = context->generation;
    return Core_operation_succeeded(context) ? RENDERER_STATUS_OK
                                             : RENDERER_STATUS_BACKEND_ERROR;
}

static void Core_destroy(void *opaque)
{
    CoreContext *context = opaque;

    if (context->context_available)
        Core_release_context_objects(context);
    free(context);
}

static const RendererBackendInterface core_backend_interface = {
    .begin_frame = Core_begin_frame,
    .draw = Core_draw,
    .flush = Core_flush,
    .end_frame = Core_end_frame,
    .texture_create = Core_texture_create,
    .texture_update = Core_texture_update,
    .texture_destroy = Core_texture_destroy,
    .mesh_create = Core_mesh_create,
    .mesh_update = Core_mesh_update,
    .mesh_destroy = Core_mesh_destroy,
    .context_lost = Core_context_lost,
    .context_restore = Core_context_restore,
    .texture_restore = Core_texture_restore,
    .mesh_restore = Core_mesh_restore,
    .destroy = Core_destroy
};

static RendererStatus Core_create(RendererGLProcLoader loader,
                                  void *userdata, Renderer **renderer,
                                  int is_es)
{
    CoreContext *context;
    Renderer *created;
    RendererStatus status;

    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    *renderer = NULL;
    if (loader == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    context = calloc(1, sizeof(*context));
    if (context == NULL)
        return RENDERER_STATUS_OUT_OF_MEMORY;
    context->loader = loader;
    context->loader_userdata = userdata;
    context->generation = 1;
    context->is_es = is_es;
    if (!Core_load_functions(&context->gl, loader, userdata, is_es)) {
        free(context);
        return RENDERER_STATUS_BACKEND_ERROR;
    }

    if (!Core_context_supported(context)) {
        free(context);
        return RENDERER_STATUS_BACKEND_ERROR;
    }
    status = Core_create_context_objects(context);
    if (status != RENDERER_STATUS_OK) {
        free(context);
        return status;
    }
    context->context_available = 1;

    created = Renderer_backend_create(&core_backend_interface, context);
    if (created == NULL) {
        Core_release_context_objects(context);
        free(context);
        return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    *renderer = created;
    return RENDERER_STATUS_OK;
}

RendererStatus Renderer_gl_core_create(RendererGLProcLoader loader,
                                       void *userdata,
                                       Renderer **renderer)
{
#ifdef __EMSCRIPTEN__
    (void)loader;
    (void)userdata;
    if (renderer != NULL)
        *renderer = NULL;
    return RENDERER_STATUS_BACKEND_ERROR;
#else
    return Core_create(loader, userdata, renderer, 0);
#endif
}

RendererStatus Renderer_gl_es_create(RendererGLProcLoader loader,
                                     void *userdata,
                                     Renderer **renderer)
{
    return Core_create(loader, userdata, renderer, 1);
}
