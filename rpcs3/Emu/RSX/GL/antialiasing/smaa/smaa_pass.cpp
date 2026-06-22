#pragma once

#include "smaa_pass.h"
#include "smaa_shader.h"
#include "textures/AreaTex.h"
#include "textures/SearchTex.h"
namespace gl
{
	smaa_pass::smaa_pass()
	{
		m_vao.create();
		m_vao.bind();
		m_vbo.create();
		m_vbo.bind();
		rsx_log.warning("Created VAO/VBO");
		glBufferData(GL_ARRAY_BUFFER, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (void*)offsetof(ScreenRectVertex, position));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (void*)offsetof(ScreenRectVertex, tex_coord));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		rsx_log.warning("Set Vertex Attribs");
		// Compile SMAA Shaders
		std::array<std::string, 3> SMAA_PASS_X_VERT_DATA;
		std::array<std::string, 3> SMAA_PASS_X_FRAG_DATA;
		SMAA_PASS_X_VERT_DATA[0] = SMAA_PASS_0_VERT;
		SMAA_PASS_X_FRAG_DATA[0] = SMAA_PASS_0_FRAG;
		SMAA_PASS_X_VERT_DATA[1] = SMAA_PASS_1_VERT;
		SMAA_PASS_X_FRAG_DATA[1] = SMAA_PASS_1_FRAG;
		SMAA_PASS_X_VERT_DATA[2] = SMAA_PASS_2_VERT;
		SMAA_PASS_X_FRAG_DATA[2] = SMAA_PASS_2_FRAG;
		for (int i = 0; i < m_vert_shader.size()-1; i++)
		{
			replaceInclude(SMAA_PASS_X_FRAG_DATA[i], "SMAA.hlsl", SMAA_HLSL);
			replaceInclude(SMAA_PASS_X_VERT_DATA[i], "SMAA.hlsl", SMAA_HLSL);
			m_vert_shader[i].create(::glsl::program_domain::glsl_vertex_program, SMAA_PASS_X_VERT_DATA[i]);
			m_vert_shader[i].compile();
			m_frag_shader[i].create(::glsl::program_domain::glsl_fragment_program, SMAA_PASS_X_FRAG_DATA[i]);
			m_frag_shader[i].compile();
			m_program[i].create();
			m_program[i].attach(m_vert_shader[i]);
			m_program[i].attach(m_frag_shader[i]);
			m_program[i].link();
		}

		// Compile Convert Color Shader (which creates linearized input for the 3rd SMAA PASSZ)
		m_vert_shader[3].create(::glsl::program_domain::glsl_vertex_program, CONVERT_COLORS_VERT);
		m_vert_shader[3].compile();
		m_frag_shader[3].create(::glsl::program_domain::glsl_fragment_program, CONVERT_COLORS_FRAG);
		m_frag_shader[3].compile();
		m_program[3].create();
		m_program[3].attach(m_vert_shader[3]);
		m_program[3].attach(m_frag_shader[3]);
		m_program[3].link();

		rsx_log.warning("Compiled Shaders");
		m_sampler[0].create();
		m_sampler[0].apply_defaults(GL_NEAREST);
		m_sampler[1].create();
		m_sampler[1].apply_defaults(GL_LINEAR);
		m_fbo.create();
		m_vertices = {
			ScreenRectVertex(-1.f, 1.f, 0.f, 1.f),  // Left,  Top
			ScreenRectVertex(1.f, 1.f, 1.f, 1.f),   // Right, Top
			ScreenRectVertex(-1.f, -1.f, 0.f, 0.f), // Left,  Bottom
			ScreenRectVertex(1.f, -1.f, 1.f, 0.f),  // Right, Bottom
		};
		rsx_log.warning("Created FBO/Sampler");
		allocateLookupTextures();
		allocateTextures(1280, 720);
		rsx_log.warning("Allocated Textures");
	}

	smaa_pass::~smaa_pass()
	{
		m_vao.remove();
		m_vbo.remove();
		for (int i = 0; i < m_vert_shader.size(); i++)
		{
			m_vert_shader[i].remove();
			m_frag_shader[i].remove();
			m_program[i].remove();
		}
		m_fbo.remove();
		m_sampler[0].remove();
		m_sampler[1].remove();
		for (int i = 0; i < m_intermediate_texture.size(); i++)
		{
			m_intermediate_texture[i].reset();
		}
		m_area_tex.reset();
		m_search_tex.reset();
		// Textures auto destroyed upon deconestruction
	}

	void smaa_pass::attachUniforms(GLuint shader_program_id)
	{
		uniform_locs.i_resolution = glGetUniformLocation(shader_program_id, "i_resolution");
		uniform_locs.convert_colors = glGetUniformLocation(shader_program_id, "convert_colors");
	}

	void smaa_pass::allocateTextures(int width, int height)
	{
		for (int i = 0; m_intermediate_texture.size(); i++)
		{
			m_intermediate_texture[i].reset();
			m_intermediate_texture[i] = std::make_unique<gl::viewable_image>(GL_TEXTURE_2D, width, height, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
		}
	}

	void smaa_pass::allocateLookupTextures()
	{
		m_area_tex = std::make_unique<gl::viewable_image>(GL_TEXTURE_2D, AREATEX_WIDTH, AREATEX_HEIGHT, 1, 1, 1, GL_RG8, RSX_FORMAT_CLASS_COLOR);
		m_search_tex = std::make_unique<gl::viewable_image>(GL_TEXTURE_2D, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1, 1, 1, GL_R8, RSX_FORMAT_CLASS_COLOR);
		glActiveTexture(GL_TEMP_IMAGE_SLOT(0));
		glBindTexture(GL_TEXTURE_2D, m_area_tex->id());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, areaTexBytes);
		glBindTexture(GL_TEXTURE_2D, m_search_tex->id());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, GL_RG, GL_UNSIGNED_BYTE, searchTexBytes);
		glBindTexture(GL_TEXTURE_2D, GL_NONE);
	}

	void smaa_pass::replaceInclude(std::string& shader_source, std::string_view include_name,
		std::string_view include_content)
	{
		const std::string include_string = fmt::format("#include \'{}\'", include_name);
		const std::size_t pos = shader_source.find(include_string);
		if (pos == std::string::npos)
		{
			rsx_log.warning("Failed to replace include: %s", include_name);
		}
		shader_source.replace(pos, include_string.size(), include_content);
	};
	gl::texture* smaa_pass::antialias_output(gl::command_context& cmd, gl::texture* src, const areai& src_region)
	{
		// TODO: Implement Texture Reallocation on Resolution Scale Change (or if resolution higher than preallocated texture)

		// Bind Framebuffer and VAO
		m_vao.bind();
		m_fbo.bind();


		// Create Linearized "SMAA_INPUT" Texture for Neighborhood Blending Pass 
		m_fbo.color = m_intermediate_texture[3]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(GL_TEMP_IMAGE_SLOT(0), m_sampler[1]);
		cmd->bind_texture(GL_TEMP_IMAGE_SLOT(0), GL_TEXTURE_2D, src->id());
		cmd->use_program(m_program[3].id());
		attachUniforms(m_program[3].id());
		glUniform1i(uniform_locs.convert_colors, 1);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// Create Edge Detection Texture
		m_fbo.color = m_intermediate_texture[0]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(GL_TEMP_IMAGE_SLOT(0), m_sampler[1]);
		cmd->bind_texture(GL_TEMP_IMAGE_SLOT(0), GL_TEXTURE_2D, src->id());
		cmd->use_program(m_program[0].id());
		attachUniforms(m_program[0].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// Create Blending Weight Calculation Texture
		m_fbo.color = m_intermediate_texture[1]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(GL_TEMP_IMAGE_SLOT(0), m_sampler[1]);
		cmd->bind_texture(GL_TEMP_IMAGE_SLOT(0), GL_TEXTURE_2D, m_intermediate_texture[0]->id());
		saved_sampler_states[1] = std::make_unique<saved_sampler_state>(GL_TEMP_IMAGE_SLOT(1), m_sampler[1]);
		cmd->bind_texture(GL_TEMP_IMAGE_SLOT(1), GL_TEXTURE_2D, m_area_tex->id());
		saved_sampler_states[2] = std::make_unique<saved_sampler_state>(GL_TEMP_IMAGE_SLOT(2), m_sampler[1]);
		cmd->bind_texture(GL_TEMP_IMAGE_SLOT(2), GL_TEXTURE_2D, m_search_tex->id());
		cmd->use_program(m_program[1].id());
		attachUniforms(m_program[1].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// Create Neighborhood Blending Texture
		m_fbo.color = m_intermediate_texture[2]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(GL_TEMP_IMAGE_SLOT(0), m_sampler[1]);
		cmd->bind_texture(GL_TEMP_IMAGE_SLOT(0), GL_TEXTURE_2D, m_intermediate_texture[1]->id());
		saved_sampler_states[1] = std::make_unique<saved_sampler_state>(GL_TEMP_IMAGE_SLOT(1), m_sampler[1]);
		cmd->bind_texture(GL_TEMP_IMAGE_SLOT(0), GL_TEXTURE_2D, m_intermediate_texture[3]->id());
		cmd->use_program(m_program[2].id());
		attachUniforms(m_program[2].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glUniform1i(uniform_locs.convert_colors, 2);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		return m_intermediate_texture[2].get();
	}
} // namespace gl
