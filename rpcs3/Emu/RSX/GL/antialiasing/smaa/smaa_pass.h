#pragma once

#include "../antialiasing.h"

namespace gl
{
	class smaa_pass : public antialiasing_filter
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
		smaa_pass();
		~smaa_pass();

		gl::texture* antialias_output(
			gl::command_context& cmd, // State
			gl::texture* src,         // Source input
			const areai& src_region   // Scaling request information
			) override;

	private:
		gl::vao m_vao;
		gl::buffer m_vbo;
		gl::fbo m_fbo;
		std::array<std::unique_ptr<saved_sampler_state>, 3> saved_sampler_states;
		std::array<gl::sampler_state, 2> m_sampler;
		std::unique_ptr<gl::viewable_image> m_area_tex;
		std::unique_ptr<gl::viewable_image> m_search_tex;
		std::array<std::unique_ptr<gl::viewable_image>, 5> m_intermediate_texture;
		std::array<gl::glsl::shader, 4> m_vert_shader;
		std::array<gl::glsl::shader, 4> m_frag_shader;
		std::array<gl::glsl::program, 4>  m_program;
		uniform_locations uniform_locs;
		void reset_sampler_states();
		void attachUniforms(GLuint shader_program_id);
		void allocateTextures(int width, int height);
		void allocateLookupTextures();
		void replaceInclude(std::string& shader_source, std::string include_name,
			std::string include_content);
		std::array<ScreenRectVertex, 4> m_vertices;
	};
} // namespace gl
