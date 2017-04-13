#pragma once
#include <glad/glad.h>
#include "hgles_buffer.h"
#include <map>
namespace hgles
{

class MGL_DLL_PUBLIC VertexArray : public Object
{
protected:
	class VAP
	{
	public:
		VAP(Buffer* b =nullptr,
			GLuint attrib_index = 0,
			GLint size = 0, GLenum type=GL_FLOAT,
			GLboolean normalized=GL_FALSE,
			GLsizei stride=0, const void *pointer=nullptr):buffer(b)
		{
			this->index = attrib_index;
			this->size = size;
			this->type = type;
			this->normalized = normalized;
			this->stride = stride;
			this->pointer = pointer;
		}
		Buffer* buffer;
		GLuint index;
		GLint size;
		GLenum type;
		GLboolean normalized;
		GLsizei stride;
		const GLvoid* pointer;
	};

	VAP m_vaps[8];
	bool m_enabled_vaa[8];
	bool m_dirty;

	Buffer* m_element_buffer;




public:
	VertexArray(ContextState* origin_context=nullptr, GLuint name=0)
		:Object(origin_context,name),m_element_buffer(nullptr)
	{
		for(uint8_t i = 0 ; i<8;i++)
		{
			m_enabled_vaa[i] = false;
		}
		m_dirty = true;

	}

	friend class ContextState;

	void elementBuffer(Buffer* b);
	void vertexAttribPointer(Buffer* b,
							 GLuint attrib_index,
							 GLint size,
							 GLenum type,
							 GLboolean normalized,
							 GLsizei stride,
							 const void *pointer);

	void enableVertexAttribArray(GLuint attrib_index);
	void disableVertexAttribArray(GLuint attrib_index);


};


}