#pragma once

#include "../upscaling.h"

namespace gl
{
	class lanczos3_pass : public upscaler
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
		lanczos3_pass();
		~lanczos3_pass();

		gl::texture* scale_output(
			gl::command_context& /*cmd*/, // State
			gl::texture* src,             // Source input
			const areai& src_region,      // Scaling request information
			const areai& dst_region,      // Ditto
			gl::flags32_t mode            // Mode
			) override;

	private:
		gl::vao m_vao;
		gl::buffer m_vbo;
		gl::fbo m_fbo;
		gl::fbo m_flip_fbo;
		std::array<std::unique_ptr<saved_sampler_state>, 1> saved_sampler_states;
		std::array<gl::sampler_state, 2> m_sampler;
		std::array<std::unique_ptr<gl::texture>, 2> m_intermediate_texture;
		std::array<gl::glsl::shader, 2> m_vert_shader;
		std::array<gl::glsl::shader, 2> m_frag_shader;
		std::array<gl::glsl::program, 2> m_program;
		areai prev_src_region = {0, 0, 1, 1};
		areai prev_dst_region = {0, 0, 1, 1};
		uniform_locations uniform_locs;
		void reset_sampler_states();
		void attachUniforms(GLuint shader_program_id);
		void allocateTextures(areai internal_res, areai screen_res);
		std::array<ScreenRectVertex, 4> m_vertices;

		GLint prev_vao;
	};
} // namespace gl
