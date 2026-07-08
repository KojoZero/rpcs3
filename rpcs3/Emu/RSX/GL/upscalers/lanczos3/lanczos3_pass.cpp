#include "lanczos3_pass.h"
#include "lanczos3_shader.h"
// FSR RCAS
#include "../shared_shaders/shared_shaders.h"

namespace gl
{
	lanczos3_pass::lanczos3_pass()
	{
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
		// Compile LANCZOS3_RCAS Shaders
		std::array<std::string, 3> LANCZOS3_RCAS_PASS_X_VERT_DATA;
		std::array<std::string, 3> LANCZOS3_RCAS_PASS_X_FRAG_DATA;
		LANCZOS3_RCAS_PASS_X_VERT_DATA[0] = LANCZOS3_PASS_0_VERT;
		LANCZOS3_RCAS_PASS_X_FRAG_DATA[0] = LANCZOS3_PASS_0_FRAG;
		LANCZOS3_RCAS_PASS_X_VERT_DATA[1] = LANCZOS3_PASS_1_VERT;
		LANCZOS3_RCAS_PASS_X_FRAG_DATA[1] = LANCZOS3_PASS_1_FRAG;
		LANCZOS3_RCAS_PASS_X_VERT_DATA[2] = FSR_PASS_1_VERT;
		LANCZOS3_RCAS_PASS_X_FRAG_DATA[2] = FSR_PASS_1_FRAG;
		replaceInclude(LANCZOS3_RCAS_PASS_X_FRAG_DATA[2], "ffx_a.h", FFX_A);
		replaceInclude(LANCZOS3_RCAS_PASS_X_FRAG_DATA[2], "ffx_fsr1.h", FFX_FSR1);

		for (size_t i = 0; i < m_vert_shader.size(); i++)
		{
			m_vert_shader[i].create(::glsl::program_domain::glsl_vertex_program, LANCZOS3_RCAS_PASS_X_VERT_DATA[i]);
			m_vert_shader[i].compile();
			m_frag_shader[i].create(::glsl::program_domain::glsl_fragment_program, LANCZOS3_RCAS_PASS_X_FRAG_DATA[i]);
			m_frag_shader[i].compile();
			m_program[i].create();
			m_program[i].attach(m_vert_shader[i]);
			m_program[i].attach(m_frag_shader[i]);
			m_program[i].link();
		}

		printf("Compiled Shaders\n");
		m_sampler[0].create();
		m_sampler[0].apply_defaults(GL_NEAREST);
		m_sampler[1].create();
		m_sampler[1].apply_defaults(GL_LINEAR);
		m_fbo.create();
		printf("Created FBO/Sampler\n");
		printf("Allocated LANCZOS3_RCAS Textures\n");
		allocateTextures(prev_src_region, prev_dst_region);
		printf("Allocated Textures\n");
		glBindVertexArray(prev_vao);
	}

	lanczos3_pass::~lanczos3_pass()
	{
		printf("Destroying: LANCZOS3_RCAS PASS\n");
		m_vao.remove();
		m_vbo.remove();
		for (size_t i = 0; i < m_vert_shader.size(); i++)
		{
			m_vert_shader[i].remove();
			m_frag_shader[i].remove();
			m_program[i].remove();
		}
		m_fbo.remove();
		m_sampler[0].remove();
		m_sampler[1].remove();
		for (size_t i = 0; i < m_intermediate_texture.size(); i++)
		{
			m_intermediate_texture[i].reset();
		}
		printf("Destroyed: LANCZOS3_RCAS PASS\n");
	}

	void lanczos3_pass::attachUniforms(GLuint shader_program_id)
	{
		uniform_locs.i_resolution = glGetUniformLocation(shader_program_id, "i_resolution");
		uniform_locs.o_resolution = glGetUniformLocation(shader_program_id, "o_resolution");
		uniform_locs.fsr_sharpening = glGetUniformLocation(shader_program_id, "FSR_SHARPENING");
	}

	void lanczos3_pass::allocateTextures(areai internal_res, areai screen_res)
	{
		m_intermediate_texture[0].reset();
		m_intermediate_texture[0] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width(), screen_res.height(), 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		printf("Allocated Intemediate Texture: %d...\n", 0);
		m_intermediate_texture[1].reset();
		m_intermediate_texture[1] = std::make_unique<gl::texture>(GL_TEXTURE_2D, screen_res.width(), screen_res.height(), 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		printf("Allocated Intemediate Texture: %d...\n", 1);
		m_intermediate_texture[2].reset();
		m_intermediate_texture[2] = std::make_unique<gl::texture>(GL_TEXTURE_2D, screen_res.width(), screen_res.height(), 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		printf("Allocated Intemediate Texture: %d...\n", 2);
	}

	void lanczos3_pass::replaceInclude(std::string& shader_source, std::string include_name,
		std::string include_content)
	{
		std::string include_string = std::format("#include \"{}\"", include_name);
		std::size_t pos = shader_source.find(include_string);
		if (pos == std::string::npos)
		{
			printf("Failed to replace include: %s, searched term: %s\n", include_name.c_str(), include_string.c_str());
		}
		shader_source.replace(pos, include_string.size(), include_content);
	};

	void lanczos3_pass::reset_sampler_states()
	{
		for (auto& sampler_state : saved_sampler_states)
		{
			sampler_state.reset();
		}
	}

	gl::texture* lanczos3_pass::scale_output(
		gl::command_context& cmd,
		gl::texture* src,
		const areai& src_region,
		const areai& dst_region,
		gl::flags32_t mode)
	{
		if (src_region.width() != prev_src_region.width() || src_region.height() != prev_src_region.height() ||
			dst_region.width() != prev_dst_region.width() || dst_region.height() != prev_dst_region.height())
		{
			allocateTextures(src_region, dst_region);
			prev_src_region = src_region;
			prev_dst_region = dst_region;
		}

		float cas_attenuation = 2.f - (g_cfg.video.rcas_sharpening_intensity.get() / 50.f);

		// Bind Framebuffer and VAO
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
		m_vao.bind();
		m_fbo.bind();

		// Lanczos Y-Pass
		m_fbo.color = m_intermediate_texture[0]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), dst_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[0]);
		cmd->bind_texture(0, GL_TEXTURE_2D, src->id());
		cmd->use_program(m_program[0].id());
		attachUniforms(m_program[0].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// Lanczos X-Pass
		m_fbo.color = m_intermediate_texture[1]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, dst_region.width(), dst_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[0]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[0]->id());
		cmd->use_program(m_program[1].id());
		attachUniforms(m_program[1].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), dst_region.height(), 1.0f / src_region.width(), 1.0f / dst_region.height());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// RCAS Pass
		m_fbo.color = m_intermediate_texture[2]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, dst_region.width(), dst_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[1]->id());
		cmd->use_program(m_program[2].id());
		attachUniforms(m_program[2].id());
		glUniform4f(uniform_locs.i_resolution, dst_region.width(), dst_region.height(), 1.0f / dst_region.width(), 1.0f / dst_region.height());
		glUniform4f(uniform_locs.o_resolution, dst_region.width(), dst_region.height(), 1.0f / dst_region.width(), 1.0f / dst_region.height());
		glUniform1f(uniform_locs.fsr_sharpening, cas_attenuation);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		glBindVertexArray(prev_vao);

		if (mode & UPSCALE_AND_COMMIT)
		{
			areai input_region = {0, 0, dst_region.width(), dst_region.height()};
			m_flip_fbo.recreate();
			m_flip_fbo.bind();
			m_flip_fbo.color = m_intermediate_texture[2]->id();
			m_flip_fbo.read_buffer(m_flip_fbo.color);
			m_flip_fbo.draw_buffer(m_flip_fbo.color);

			m_flip_fbo.blit(gl::screen, input_region, dst_region, gl::buffers::color, gl::filter::linear);
			return 0;
		}

		return m_intermediate_texture[2].get();
	}
} // namespace gl
