//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#include "wglext.h"
#else
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#include "glcorearb.h" //core opengl

#include "core_utils.hpp"

namespace KalaGraphics::Internal::OpenGL
{
    using u8 = uint8_t;
    using u32 = uint32_t;

#ifdef __linux__
	typedef void (*PFNGLDRAWARRAYSPROC)(
		GLenum mode, 
		GLint first, 
		GLsizei count);
	typedef void (*PFNGLDRAWELEMENTSPROC)(
		GLenum mode, 
		GLsizei count, 
		GLenum type, 
		const void* indices);
	typedef void (*PFNGLBINDTEXTUREPROC)(
		GLenum target, 
		GLuint texture);
	typedef void (*PFNGLDELETETEXTURESPROC)(
		GLsizei n, 
		const GLuint* textures);
	typedef void (*PFNGLGENTEXTURESPROC)(
		GLsizei n, 
		GLuint* textures);
	typedef void (*PFNGLTEXSUBIMAGE2DPROC)(
		GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		const void* pixels);
#endif

    struct LIB_API OpenGL_Core_Functions
    {
		//
		// GEOMETRY
		//

		//Maps a range of a buffer object's data store into the client's address space
		PFNGLMAPBUFFERRANGEPROC glMapBufferRange;

		//Creates an immutable data store for a buffer object (ARB_buffer_storage)
		PFNGLBUFFERSTORAGEPROC glBufferStorage;

		//Binds a named buffer to a specified buffer binding point
		PFNGLBINDBUFFERPROC glBindBuffer;

		//Binds a vertex array object
		PFNGLBINDVERTEXARRAYPROC glBindVertexArray;

		//Creates and initializes a buffer object's data store
		PFNGLBUFFERDATAPROC glBufferData;

		//Updates existing buffer object's data without reallocating storage
		PFNGLBUFFERSUBDATAPROC glBufferSubData;

		//Deletes one or more named buffer objects
		PFNGLDELETEBUFFERSPROC glDeleteBuffers;

		//Deletes one or more named vertex array objects
		PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;

		//Draws non-indexed primitives from array data
		PFNGLDRAWARRAYSPROC glDrawArrays;

		//Draws indexed primitives using array data and element indices
		PFNGLDRAWELEMENTSPROC glDrawElements;

		//Enables a generic vertex attribute array
		PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;

		//Generates buffer object names
		PFNGLGENBUFFERSPROC glGenBuffers;

		//Generates vertex array object names
		PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;

		//Retrieves parameter values for a vertex attribute array
		PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;

		//Retrieves a pointer to a vertex attribute array parameter
		PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointerv;

		//Defines an array of generic vertex attribute data
		PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;

		//Disables a generic vertex attribute array
		PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;

		//Sets a constant integer value for a generic vertex attribute
		PFNGLVERTEXATTRIBI1IPROC glVertexAttribI1i;

		//Sets a constant integer vec2 value for a generic vertex attribute
		PFNGLVERTEXATTRIBI2IPROC glVertexAttribI2i;

		//Sets a constant integer vec3 value for a generic vertex attribute
		PFNGLVERTEXATTRIBI3IPROC glVertexAttribI3i;

		//Sets a constant integer vec4 value for a generic vertex attribute
		PFNGLVERTEXATTRIBI4IPROC glVertexAttribI4i;

		//Tells OpenGL which faces to not render before fragment shading
		PFNGLCULLFACEPROC glCullFace;

        //
        // SHADERS
        //

		//Attaches a shader object to a program
		PFNGLATTACHSHADERPROC glAttachShader;

		//Compiles a shader object
		PFNGLCOMPILESHADERPROC glCompileShader;

		//Creates a new shader program object
		PFNGLCREATEPROGRAMPROC glCreateProgram;

		//Creates a shader object of the specified type (GL_VERTEX_SHADER, etc.)
		PFNGLCREATESHADERPROC glCreateShader;

		//Deletes a shader object
		PFNGLDELETESHADERPROC glDeleteShader;

		//Deletes a program object
		PFNGLDELETEPROGRAMPROC glDeleteProgram;

		//Detaches a shader object from a program
		PFNGLDETACHSHADERPROC glDetachShader;

		//Retrieves information about an active attribute variable
		PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;

		//Returns the attribute location within a shader program
		PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;

		//Retrieves a parameter from a program object
		PFNGLGETPROGRAMIVPROC glGetProgramiv;

		//Returns the information log for a program object
		PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;

		//Retrieves a parameter from a shader object
		PFNGLGETSHADERIVPROC glGetShaderiv;

		//Returns the information log for a shader object
		PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;

		//Links a program object
		PFNGLLINKPROGRAMPROC glLinkProgram;

		//Sets the source code for a shader
		PFNGLSHADERSOURCEPROC glShaderSource;

		//Activates a shader program for rendering
		PFNGLUSEPROGRAMPROC glUseProgram;

		//Validates a program object to see if it's executable
		PFNGLVALIDATEPROGRAMPROC glValidateProgram;

		//Returns whether a given program name is a valid program object
		PFNGLISPROGRAMPROC glIsProgram;

        //
        // UNIFORMS
        //

        //Retrieves the location of a uniform variable within a shader program
		PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;

		//Find block index by name
		PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;

		//Bind block index to binding point
		PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;

        //Sets a float uniform
		PFNGLUNIFORM1FPROC glUniform1f;

		//Sets a integer uniform
		PFNGLUNIFORM1IPROC glUniform1i;

		//Sets a float uniform from an array
		PFNGLUNIFORM1FVPROC glUniform1fv;

		//Sets a int uniform from an array
		PFNGLUNIFORM1IVPROC glUniform1iv;

		//Sets a vec2 float uniform
		PFNGLUNIFORM2FPROC glUniform2f;

		//Sets a vec2 integer uniform
		PFNGLUNIFORM2IPROC glUniform2i;

		//Sets a vec2 float uniform from an array
		PFNGLUNIFORM2FVPROC glUniform2fv;

		//Sets a vec2 int uniform from an array
		PFNGLUNIFORM2IVPROC glUniform2iv;

		//Sets a vec3 float uniform
		PFNGLUNIFORM3FPROC glUniform3f;

		//Sets a vec3 integer uniform
		PFNGLUNIFORM3IPROC glUniform3i;

		//Sets a vec3 float uniform from an array
		PFNGLUNIFORM3FVPROC glUniform3fv;

		//Sets a vec3 int uniform from an array
		PFNGLUNIFORM3IVPROC glUniform3iv;

		//Sets a vec4 float uniform
		PFNGLUNIFORM4FPROC glUniform4f;

		//Sets a vec4 integer uniform
		PFNGLUNIFORM4IPROC glUniform4i;

		//Sets a vec4 float uniform from an array
		PFNGLUNIFORM4FVPROC glUniform4fv;

		//Sets a vec4 int uniform from an array
		PFNGLUNIFORM4IVPROC glUniform4iv;

		//Sets a 2x2 matrix uniform from an array of floats
		PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;

		//Sets a 3x3 matrix uniform from an array of floats
		PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;

		//Sets a 4x4 matrix uniform from an array of floats
		PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
    };

#ifdef _WIN32
    struct LIB_API OpenGL_Windows_Functions
    {
        PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
    };
#else
    struct LIB_API OpenGL_Linux_Functions
    {
        PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;
    };
#endif

    class LIB_API OpenGL_Core
    {
    public:
        static void SetCoreFunctions(const OpenGL_Core_Functions& coreFunc);
        static const OpenGL_Core_Functions& GetCoreFunctions();
#ifdef _WIN32
        static void SetWindowsFunctions(const OpenGL_Windows_Functions& winFunc);
        static const OpenGL_Windows_Functions& GetWindowsFunctions();
#else
        static void SetLinuxFunctions(const OpenGL_Linux_Functions& linFunc);
        static const OpenGL_Linux_Functions& GetLinuxFunctions();
#endif

		//Make the GL context correct for the current window
		static void MakeContextCurrent(u32 contextID);

		//Confirms that the GL context is the same as the stored context for this window
		static bool IsContextValid(u32 contextID);

        //Main draw call
        static void Update(u32 contextID);
        //Actions that occur only when the window size changes
        static void ResizeUpdate(u32 contextID);
    
        static bool SetVSyncState(
            u32 contextID,
            u8 newValue);

        //Called at the end of each frame
		static void SwapOpenGLBuffers(u32 contextID);

        //Clean all resources
        static void Shutdown(u32 contextID);
    };
}