/* Standard GLES 1.1 ABI subset imported from the NanoApps loader. */
#ifndef NANO_GL_API_H
#define NANO_GL_API_H
typedef unsigned int GLenum, GLbitfield, GLuint;
typedef int GLint, GLsizei, GLsizeiptr;
typedef unsigned char GLboolean, GLubyte;
typedef float GLfloat, GLclampf;
typedef void GLvoid;
#define GL_ADD 0x0104
#define GL_ARRAY_BUFFER 0x8892
#define GL_BLEND 0x0BE2
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_COLOR_ARRAY 0x8076
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_DEPTH_TEST 0x0B71
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FLOAT 0x1406
#define GL_GREATER 0x0204
#define GL_LEQUAL 0x0203
#define GL_LIGHTING 0x0B50
#define GL_LINEAR 0x2601
#define GL_MODELVIEW 0x1700
#define GL_MODULATE 0x2100
#define GL_NEAREST 0x2600
#define GL_NORMAL_ARRAY 0x8075
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 0x0001
#define GL_PROJECTION 0x1701
#define GL_REPEAT 0x2901
#define GL_REPLACE 0x1E01
#define GL_RGBA 0x1908
#define GL_SCISSOR_TEST 0x0C11
#define GL_SRC_ALPHA 0x0302
#define GL_SRC_COLOR 0x0300
#define GL_TEXTURE 0x1702
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_COLOR 0x2201
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TRIANGLES 0x0004
#define GL_TRUE 1
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNSIGNED_BYTE 0x1401
#define GL_VERTEX_ARRAY 0x8074
void glActiveTexture(GLenum texture);
void glAlphaFunc(GLenum func, GLclampf ref);
void glBindBuffer(GLenum target, GLuint buffer);
void glBindTexture(GLenum target, GLuint texture);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data,
                  GLenum usage);
void glClear(GLbitfield mask);
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glClientActiveTexture(GLenum texture);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void glDeleteBuffers(GLsizei n, const GLuint *buffers);
void glDeleteTextures(GLsizei n, const GLuint *textures);
void glDepthFunc(GLenum func);
void glDepthMask(GLboolean flag);
void glDisable(GLenum cap);
void glDisableClientState(GLenum array);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glEnable(GLenum cap);
void glEnableClientState(GLenum array);
void glGenBuffers(GLsizei n, GLuint *buffers);
void glGenTextures(GLsizei n, GLuint *textures);
GLenum glGetError(void);
void glGetIntegerv(GLenum pname, GLint *params);
void glLoadIdentity(void);
void glMatrixMode(GLenum mode);
void glPixelStorei(GLenum pname, GLint param);
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride,
                       const GLvoid *ptr);
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);
void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border, GLenum format,
                  GLenum type, const GLvoid *pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glVertexPointer(GLint size, GLenum type, GLsizei stride,
                     const GLvoid *ptr);
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
#endif
