#include "fxaa_pass.h"
#include "fxaa_shader.h"
namespace gl
{
	fxaa_pass::fxaa_pass() {
		m_vertices = {
			ScreenRectVertex(-1.f, 1.f, 0.f, 1.f),  // Left,  Top
			ScreenRectVertex(1.f, 1.f, 1.f, 1.f),   // Right, Top
			ScreenRectVertex(-1.f, -1.f, 0.f, 0.f), // Left,  Bottom
			ScreenRectVertex(1.f, -1.f, 1.f, 0.f),  // Right, Bottom
		};
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
		m_vao.create();
		m_vao.bind();
		m_vbo.create(sizeof(ScreenRectVertex) * 4, m_vertices.data(), gl::buffer::memory_type::local, 0);
		m_vbo.bind();
		printf("Created VAO/VBO\n");
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), reinterpret_cast<void*>(offsetof(ScreenRectVertex, position)));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), reinterpret_cast<void*>(offsetof(ScreenRectVertex, tex_coord)));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		printf("Set Vertex Attribs\n");
		m_vert_shader.create(::glsl::program_domain::glsl_vertex_program, FXAA_VERT);
		m_vert_shader.compile();
		m_frag_shader.create(::glsl::program_domain::glsl_fragment_program, FXAA_FRAG);
		m_frag_shader.compile();
		m_program.create();
		m_program.attach(m_vert_shader);
		m_program.attach(m_frag_shader);
		m_program.link();
		printf("Compiled Shader\n");
		m_sampler.create();
		m_sampler.apply_defaults(GL_LINEAR);
		m_fbo.create();
		printf("Created FBO/Sampler\n");
		allocateTextures(prev_src_region);
		printf("Allocated Textures\n");
		glBindVertexArray(prev_vao);
	}


	fxaa_pass::~fxaa_pass() {
		printf("Destroying: FXAA PASS\n");
		m_vao.remove();
		m_vbo.remove();
		m_vert_shader.remove();
		m_frag_shader.remove();
		m_program.remove();
		m_fbo.remove();
		m_sampler.remove();
		m_intermediate_texture.reset();
		printf("Destroyed: FXAA PASS\n");
	}

	void fxaa_pass::attachUniforms(GLuint shader_program_id) {
		uniform_locs.i_resolution = glGetUniformLocation(shader_program_id, "i_resolution");
		uniform_locs.convert_colors = glGetUniformLocation(shader_program_id, "convert_colors");
	}

	void fxaa_pass::allocateTextures(areai internal_res)
	{
		m_intermediate_texture.reset();
		m_intermediate_texture = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width(), internal_res.height(), 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
	}
	void fxaa_pass::saveGLState()
	{
		m_prevGLState.blend_enabled = glIsEnabledi(GL_BLEND, 0);
		m_prevGLState.cull_enabled = glIsEnabled(GL_CULL_FACE);
		glGetIntegeri_v(GL_BLEND_SRC_RGB, 0, &m_prevGLState.src_rgb);
		glGetIntegeri_v(GL_BLEND_DST_RGB, 0, &m_prevGLState.dst_rgb);
		glGetIntegeri_v(GL_BLEND_SRC_ALPHA, 0, &m_prevGLState.src_alpha);
		glGetIntegeri_v(GL_BLEND_DST_ALPHA, 0, &m_prevGLState.dst_alpha);
		glGetIntegeri_v(GL_BLEND_EQUATION_RGB, 0, &m_prevGLState.eq_rgb);
		glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA, 0, &m_prevGLState.eq_alpha);
	}

	void fxaa_pass::restoreGLState()
	{
		glBlendFuncSeparatei(0, m_prevGLState.src_rgb, m_prevGLState.dst_rgb,
			m_prevGLState.src_alpha, m_prevGLState.dst_alpha);
		glBlendEquationSeparatei(0, m_prevGLState.eq_rgb, m_prevGLState.eq_alpha);
		if (m_prevGLState.blend_enabled)
		{
			glEnablei(GL_BLEND, 0);
		}
		else
		{
			glDisablei(GL_BLEND, 0);
		}
		if (m_prevGLState.cull_enabled)
		{
			glEnable(GL_CULL_FACE);
		}
		else
		{
			glDisable(GL_CULL_FACE);
		}
	}
	gl::texture* fxaa_pass::antialias_output(gl::command_context& cmd, gl::texture* src, const areai& src_region) {
		if (src_region.width() != prev_src_region.width() || src_region.height() != prev_src_region.height())
		{
			allocateTextures(src_region);
			prev_src_region = src_region;
		}

		// Bind Framebuffer and VAO
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
		m_vao.bind();
		m_fbo.bind();
		saveGLState();
		cmd->disablei(GL_BLEND, 0);
		cmd->disable(GL_CULL_FACE);
		// Start Antialiasing
		m_fbo.color = m_intermediate_texture->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_state saved(0, m_sampler);
		cmd->bind_texture(0, GL_TEXTURE_2D, src->id());
		cmd->use_program(m_program.id());
		attachUniforms(m_program.id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glUniform1i(uniform_locs.convert_colors, 0);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(prev_vao);

		//restoreGLState();
		return m_intermediate_texture.get();
	}
} // namespace gl
