#pragma once

#include "../antialiasing.h"

namespace gl
{
	class fxaa_pass : public antialiasing_filter
	{
	struct uniform_locations
	{
		GLint i_resolution;
		GLint convert_colors;
	};
	public:
		fxaa_pass() = default;
		~fxaa_pass();

		gl::texture* antialias_output(
			gl::command_context& cmd, // State
			gl::texture* src,         // Source input
			const areai& src_region  // Scaling request information
			) override;

	private:
		//std::unique_ptr<gl::viewable_image> m_output_left;
		//std::unique_ptr<gl::viewable_image> m_output_right;
		//std::unique_ptr<gl::viewable_image> m_intermediate_data;

		gl::fbo m_fbo;
		gl::sampler_state m_sampler;
		std::unique_ptr<gl::texture> m_intermediate_texture;
		gl::glsl::shader m_vert_shader;
		gl::glsl::shader m_frag_shader;
		gl::glsl::program m_program;
		uniform_locations uniform_locs;
		void attachUniforms();
		void allocateTexture(int width, int height);


	};
} // namespace gl
