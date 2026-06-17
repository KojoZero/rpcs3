#pragma once

#include "../antialiasing.h"
namespace gl
{
	class fxaa_pass : public antialiasing_filter
	{
	public:
		fxaa_pass() = default;
		~fxaa_pass();

		gl::texture* antialias_output(
			gl::command_context& cmd, // State
			gl::texture* src,         // Source input
			const areai& src_region  // Scaling request information
			) override;

	private:
		std::unique_ptr<gl::viewable_image> m_output_left;
		std::unique_ptr<gl::viewable_image> m_output_right;
		std::unique_ptr<gl::viewable_image> m_intermediate_data;

		gl::fbo m_texture_fbo;
		gl::sampler_state m_sampler;
		gl::texture m_antialiased_texture;
	};
} // namespace gl
