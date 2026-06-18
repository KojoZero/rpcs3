#pragma once

#include "fxaa_pass.h"
#include "fxaa_shader.h"
namespace gl
{
	fxaa_pass::fxaa_pass() {
		m_fbo.create();
		m_vert_shader.create(::glsl::program_domain::glsl_vertex_program, FXAA_VERT);
		m_vert_shader.compile();
		m_frag_shader.create(::glsl::program_domain::glsl_fragment_program, FXAA_FRAG);
		m_frag_shader.compile();
		m_program.create();
		m_program.attach(m_vert_shader);
		m_program.attach(m_frag_shader);
		m_program.link();
		attachUniforms();
		allocateTexture(1280, 720);
	}


	fxaa_pass::~fxaa_pass() {
		m_vert_shader.remove();
		m_frag_shader.remove();
		m_program.remove();
		m_fbo.remove();
	}

	void fxaa_pass::attachUniforms() {
		uniform_locs.i_resolution  = glGetUniformLocation(m_program.id(), "i_resolution");
		uniform_locs.convert_colors = glGetUniformLocation(m_program.id(), "convert_colors");
	}

	void fxaa_pass::allocateTexture(int width, int height) {
		m_intermediate_texture.reset();
		m_intermediate_texture = std::make_unique<gl::texture>(GL_TEXTURE_2D, width, height, 1, 1, 1, GL_RGBA16F, 0);
	}

	gl::texture* fxaa_pass::antialias_output(gl::command_context& cmd, gl::texture* src, const areai& src_region) {
		m_fbo.bind();
		// Allocate Texture if resolution was changed
		m_fbo.attach_texture(m_intermediate_texture.get());
		cmd->clear_color(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		cmd->use_program(m_program.id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glUniform1i(uniform_locs.convert_colors, 0);
	}
} // namespace gl
