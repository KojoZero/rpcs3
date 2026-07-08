#include "nnedi3_pass.h"
#include "nnedi3_shader.h"
// FSR RCAS
#include "../shared_shaders/shared_shaders.h"

namespace gl
{
	nnedi3_pass::nnedi3_pass()
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
		// Compile NNEDI3 Shaders
		std::array<std::string, 9> NNEDI3_PASS_X_VERT_DATA;
		std::array<std::string, 9> NNEDI3_PASS_X_FRAG_DATA;


		NNEDI3_PASS_X_VERT_DATA[0] = RGB_TO_YUV_VERT;
		NNEDI3_PASS_X_FRAG_DATA[0] = RGB_TO_YUV_FRAG;
		NNEDI3_PASS_X_VERT_DATA[1] = NNEDI3_PASS_0_VERT;
		NNEDI3_PASS_X_FRAG_DATA[1] = NNEDI3_PASS_0_FRAG;
		NNEDI3_PASS_X_VERT_DATA[2] = NNEDI3_PASS_1_VERT;
		NNEDI3_PASS_X_FRAG_DATA[2] = NNEDI3_PASS_1_FRAG;
		NNEDI3_PASS_X_VERT_DATA[3] = CENTER_SHIFT_VERT;
		NNEDI3_PASS_X_FRAG_DATA[3] = CENTER_SHIFT_FRAG;
		NNEDI3_PASS_X_VERT_DATA[4] = BICUBIC_MITCHELL_PASS_0_VERT;
		NNEDI3_PASS_X_FRAG_DATA[4] = BICUBIC_MITCHELL_PASS_0_FRAG;
		NNEDI3_PASS_X_VERT_DATA[5] = BICUBIC_MITCHELL_PASS_1_VERT;
		NNEDI3_PASS_X_FRAG_DATA[5] = BICUBIC_MITCHELL_PASS_1_FRAG;
		NNEDI3_PASS_X_VERT_DATA[6] = YUV_TO_RGB_2X_VERT;
		NNEDI3_PASS_X_FRAG_DATA[6] = YUV_TO_RGB_2X_FRAG;
		NNEDI3_PASS_X_VERT_DATA[7] = CATMULL_ROM_VERT;
		NNEDI3_PASS_X_FRAG_DATA[7] = CATMULL_ROM_FRAG;
		NNEDI3_PASS_X_VERT_DATA[8] = FSR_PASS_1_VERT;
		NNEDI3_PASS_X_FRAG_DATA[8] = FSR_PASS_1_FRAG;
		replaceInclude(NNEDI3_PASS_X_FRAG_DATA[8], "ffx_a.h", FFX_A);
		replaceInclude(NNEDI3_PASS_X_FRAG_DATA[8], "ffx_fsr1.h", FFX_FSR1);

		for (size_t i = 0; i < m_vert_shader.size(); i++)
		{
			m_vert_shader[i].create(::glsl::program_domain::glsl_vertex_program, NNEDI3_PASS_X_VERT_DATA[i]);
			m_vert_shader[i].compile();
			m_frag_shader[i].create(::glsl::program_domain::glsl_fragment_program, NNEDI3_PASS_X_FRAG_DATA[i]);
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
		printf("Allocated NNEDI3 Textures\n");
		allocateTextures(prev_src_region, prev_dst_region);
		printf("Allocated Textures\n");
		glBindVertexArray(prev_vao);
	}

	nnedi3_pass::~nnedi3_pass()
	{
		printf("Destroying: NNEDI3 PASS\n");
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
		printf("Destroyed: NNEDI3 PASS\n");
	}

	void nnedi3_pass::attachUniforms(GLuint shader_program_id)
	{
		uniform_locs.i_resolution = glGetUniformLocation(shader_program_id, "i_resolution");
		uniform_locs.o_resolution = glGetUniformLocation(shader_program_id, "o_resolution");
		uniform_locs.fsr_sharpening = glGetUniformLocation(shader_program_id, "FSR_SHARPENING");
	}

	void nnedi3_pass::allocateTextures(areai internal_res, areai screen_res)
	{
		// RGB -> YUV
		m_intermediate_texture[0].reset();
		m_intermediate_texture[0] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width(), internal_res.height(), 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// NNEDI3 Y
		m_intermediate_texture[1].reset();
		m_intermediate_texture[1] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width(), internal_res.height()*2, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// NNEDI3 X
		m_intermediate_texture[2].reset();
		m_intermediate_texture[2] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width()*2, internal_res.height()*2, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// CENTER SHIFT
		m_intermediate_texture[3].reset();
		m_intermediate_texture[3] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width()*2, internal_res.height()*2, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// Bicubic Y
		m_intermediate_texture[4].reset();
		m_intermediate_texture[4] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width(), internal_res.height()*2, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// Bicubic X
		m_intermediate_texture[5].reset();
		m_intermediate_texture[5] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width()*2, internal_res.height()*2, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// YUV -> RGB
		m_intermediate_texture[6].reset();
		m_intermediate_texture[6] = std::make_unique<gl::texture>(GL_TEXTURE_2D, internal_res.width()*2, internal_res.height()*2, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// Catmull Rom
		m_intermediate_texture[7].reset();
		m_intermediate_texture[7] = std::make_unique<gl::texture>(GL_TEXTURE_2D, screen_res.width(), screen_res.height(), 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		// FSR RCAS
		m_intermediate_texture[8].reset();
		m_intermediate_texture[8] = std::make_unique<gl::texture>(GL_TEXTURE_2D, screen_res.width(), screen_res.height(), 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
	}

	void nnedi3_pass::replaceInclude(std::string& shader_source, std::string include_name,
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

	void nnedi3_pass::reset_sampler_states()
	{
		for (auto& sampler_state : saved_sampler_states)
		{
			sampler_state.reset();
		}
	}

	gl::texture* nnedi3_pass::scale_output(
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

		/*
		Implementation Details
		1. Convert image from RGB to YUV
		2. Use NNEDI3 (2x upscale) on the luma (Y) channel (followed by center shift to correct the minor shift)
		3. Use Bicubic (Mitchell) (2x upscale) on the chroma (UV) channels.
		4. Combine the upscaled luma channel with the upscaled chroma channels
		5. Rescale the combined result with Catmull-Rom and then finally use RCAS for sharpening with halo protctions


		Notes:
			This implementation uses 16 neuron version on NNEDI3 to keep it as cheap as possible

			NNEDI3 is run on the luma channel as it packs the most of the information relevant to perceptive quality
			at 1/3 the cost of doing this on rgb.

			Bicubic (Mitchell) is run on the chroma channels as it's a lighter weight smoothing algorithm which hides
			some aliasing which would otherwise be prominent
		*/


		// Bind Framebuffer and VAO
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
		m_vao.bind();
		m_fbo.bind();

		// RGB -> YUV
		m_fbo.color = m_intermediate_texture[0]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, src->id());
		cmd->use_program(m_program[0].id());
		attachUniforms(m_program[0].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glUniform4f(uniform_locs.o_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// NNEDI3 Y
		m_fbo.color = m_intermediate_texture[1]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height() * 2);
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[0]->id());
		cmd->use_program(m_program[1].id());
		attachUniforms(m_program[1].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glUniform4f(uniform_locs.o_resolution, src_region.width(), src_region.height()*2, 1.0f / src_region.width(), 1.0f / (src_region.height()*2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// NNEDI3 X
		m_fbo.color = m_intermediate_texture[2]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width() * 2, src_region.height() * 2);
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[1]->id());
		cmd->use_program(m_program[2].id());
		attachUniforms(m_program[2].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height() * 2, 1.0f / src_region.width(), 1.0f / (src_region.height() * 2));
		glUniform4f(uniform_locs.o_resolution, src_region.width() * 2, src_region.height() * 2, 1.0f / (src_region.width()*2), 1.0f / (src_region.height() * 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// CENTER SHIFT
		m_fbo.color = m_intermediate_texture[3]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width() * 2, src_region.height() * 2);
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[2]->id());
		cmd->use_program(m_program[3].id());
		attachUniforms(m_program[3].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width() * 2, src_region.height() * 2, 1.0f / (src_region.width() * 2), 1.0f / (src_region.height() * 2));
		glUniform4f(uniform_locs.o_resolution, src_region.width() * 2, src_region.height() * 2, 1.0f / (src_region.width() * 2), 1.0f / (src_region.height() * 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// Bicubic Y
		m_fbo.color = m_intermediate_texture[4]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height() * 2);
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[3]->id());
		cmd->use_program(m_program[4].id());
		attachUniforms(m_program[4].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glUniform4f(uniform_locs.o_resolution, src_region.width(), src_region.height() * 2, 1.0f / src_region.width(), 1.0f / (src_region.height() * 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// Bicubic X
		m_fbo.color = m_intermediate_texture[5]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width() * 2, src_region.height() * 2);
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[4]->id());
		cmd->use_program(m_program[5].id());
		attachUniforms(m_program[5].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height() * 2, 1.0f / src_region.width(), 1.0f / (src_region.height() * 2));
		glUniform4f(uniform_locs.o_resolution, src_region.width() * 2, src_region.height() * 2, 1.0f / (src_region.width() * 2), 1.0f / (src_region.height() * 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// YUV -> RGB
		m_fbo.color = m_intermediate_texture[6]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width() * 2, src_region.height() * 2);
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		saved_sampler_states[1] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[5]->id());
		cmd->bind_texture(1, GL_TEXTURE_2D, m_intermediate_texture[3]->id());
		cmd->use_program(m_program[6].id());
		attachUniforms(m_program[6].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width() * 2, src_region.height() * 2, 1.0f / (src_region.width() * 2), 1.0f / (src_region.height() * 2));
		glUniform4f(uniform_locs.o_resolution, src_region.width() * 2, src_region.height() * 2, 1.0f / (src_region.width() * 2), 1.0f / (src_region.height() * 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// Catmull Rom
		m_fbo.color = m_intermediate_texture[7]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, dst_region.width(), dst_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[6]->id());
		cmd->use_program(m_program[7].id());
		attachUniforms(m_program[7].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width() * 2, src_region.height() * 2, 1.0f / (src_region.width() * 2), 1.0f / (src_region.height() * 2));
		glUniform4f(uniform_locs.o_resolution, dst_region.width(), dst_region.height(), 1.0f / (dst_region.width()), 1.0f / (dst_region.height()));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// FSR RCAS
		m_fbo.color = m_intermediate_texture[8]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, dst_region.width(), dst_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[7]->id());
		cmd->use_program(m_program[8].id());
		attachUniforms(m_program[8].id());
		glUniform4f(uniform_locs.o_resolution, dst_region.width(), dst_region.height(), 1.0f / (dst_region.width()), 1.0f / (dst_region.height()));
		glUniform4f(uniform_locs.o_resolution, dst_region.width(), dst_region.height(), 1.0f / (dst_region.width()), 1.0f / (dst_region.height()));
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
			m_flip_fbo.color = m_intermediate_texture[8]->id();
			m_flip_fbo.read_buffer(m_flip_fbo.color);
			m_flip_fbo.draw_buffer(m_flip_fbo.color);

			m_flip_fbo.blit(gl::screen, input_region, dst_region, gl::buffers::color, gl::filter::linear);
			return 0;
		}

		return m_intermediate_texture[8].get();
	}
} // namespace gl
