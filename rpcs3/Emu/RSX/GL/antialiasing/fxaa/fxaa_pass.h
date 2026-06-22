#pragma once

#include "../antialiasing.h"

namespace gl
{
	class fxaa_pass : public antialiasing_filter
	{
	struct uniform_locations
	{
		GLuint i_resolution;
		GLuint convert_colors;
	};
	struct ScreenRectVertex
	{
		ScreenRectVertex() = default;
		ScreenRectVertex(GLfloat x, GLfloat y, GLfloat u, GLfloat v)
		{
			position[0] = x;
			position[1] = y;
			tex_coord[0] = u;
			tex_coord[1] = v;
		}

		std::array<GLfloat, 2> position{};
		std::array<GLfloat, 2> tex_coord{};
	};
	public:
		fxaa_pass();
		~fxaa_pass();

		gl::texture* antialias_output(
			gl::command_context& cmd, // State
			gl::texture* src,         // Source input
			const areai& src_region  // Scaling request information
			) override;

	private:
		gl::vao m_vao;
		gl::buffer m_vbo;
		gl::fbo m_fbo;
		gl::sampler_state m_sampler;
		std::unique_ptr<gl::viewable_image> m_intermediate_texture;
		gl::glsl::shader m_vert_shader;
		gl::glsl::shader m_frag_shader;
		gl::glsl::program m_program;
		uniform_locations uniform_locs;
		void attachUniforms(GLuint shader_program_id);
		void allocateTextures(int width, int height);
		std::array<ScreenRectVertex, 4> m_vertices;
	};
} // namespace gl
