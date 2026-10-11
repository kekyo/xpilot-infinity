/* OpenGL ES 3.0 renderer construction contract. */

#ifndef RENDERER_GL_ES_H
#define RENDERER_GL_ES_H

#include "renderer_gl.h"

/**
 * Create a shader/VBO backend borrowing the current OpenGL ES 3.0 context.
 *
 * @param loader Resolver associated with the current context.
 * @param userdata Value passed to loader.
 * @param renderer Receives the renderer on success.
 * @return Operation status.
 *
 * @remarks The active context must remain current during renderer operations.
 * Notify the renderer before context loss and restore it after a compatible
 * replacement becomes current. The backend does not own the context.
 */
RendererStatus Renderer_gl_es_create(RendererGLProcLoader loader,
                                     void *userdata,
                                     Renderer **renderer);

#endif /* RENDERER_GL_ES_H */
