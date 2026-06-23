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
		printf("Created VAO/VBO\n");
		glBufferData(GL_ARRAY_BUFFER, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (void*)offsetof(ScreenRectVertex, position));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (void*)offsetof(ScreenRectVertex, tex_coord));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		printf("Set Vertex Attribs\n");
		// Compile SMAA Shaders
		std::array<std::string, 3> SMAA_PASS_X_VERT_DATA;
		std::array<std::string, 3> SMAA_PASS_X_FRAG_DATA;
		SMAA_PASS_X_VERT_DATA[0] = SMAA_PASS_0_VERT;
		SMAA_PASS_X_FRAG_DATA[0] = SMAA_PASS_0_FRAG;
		SMAA_PASS_X_VERT_DATA[1] = SMAA_PASS_1_VERT;
		SMAA_PASS_X_FRAG_DATA[1] = SMAA_PASS_1_FRAG;
		SMAA_PASS_X_VERT_DATA[2] = SMAA_PASS_2_VERT;
		SMAA_PASS_X_FRAG_DATA[2] = SMAA_PASS_2_FRAG;
		for (size_t i = 0; i < m_vert_shader.size()-1; i++)
		{
			printf("Compiling SMAA_PASS_%d_VERT...\n", static_cast<int>(i));
			replaceInclude(SMAA_PASS_X_VERT_DATA[i], "SMAA.hlsl", SMAA_HLSL);
			printf("Compiling SMAA_PASS_%d_FRAG...\n", static_cast<int>(i));
			replaceInclude(SMAA_PASS_X_FRAG_DATA[i], "SMAA.hlsl", SMAA_HLSL);
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

		printf("Compiled Shaders\n");
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
		printf("Created FBO/Sampler\n");
		allocateLookupTextures();
		printf("Allocated SMAA Textures\n");
		allocateTextures(1280, 720);
		printf("Allocated Textures\n");
	}

	smaa_pass::~smaa_pass()
	{
		printf("Destroying: SMAA PASS\n");
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
		m_area_tex.reset();
		m_search_tex.reset();
		printf("Destroyed: SMAA PASS\n");
	}

	void smaa_pass::attachUniforms(GLuint shader_program_id)
	{
		uniform_locs.i_resolution = glGetUniformLocation(shader_program_id, "i_resolution");
		uniform_locs.convert_colors = glGetUniformLocation(shader_program_id, "convert_colors");
	}

	void smaa_pass::allocateTextures(int width, int height)
	{
		for (size_t i = 0; i < m_intermediate_texture.size(); i++)
		{
			m_intermediate_texture[i].reset();
			m_intermediate_texture[i] = std::make_unique<gl::texture>(GL_TEXTURE_2D, width, height, 1, 1, 1, GL_RGBA16F, RSX_FORMAT_CLASS_COLOR);
			printf("Allocated Intemediate Texture: %d...\n", static_cast<int>(i));
		}
	}
	std::vector<unsigned char> flipVertically(const unsigned char* data, int width, int height, int channels)
	{
		int rowSize = width * channels;
		std::vector<unsigned char> flipped(width * height * channels);

		for (int y = 0; y < height; y++)
		{
			const unsigned char* src = data + (height - 1 - y) * rowSize;
			unsigned char* dst = flipped.data() + y * rowSize;
			memcpy(dst, src, rowSize);
		}

		return flipped;
	}

	void smaa_pass::allocateLookupTextures()
	{
		m_area_tex = std::make_unique<gl::texture>(GL_TEXTURE_2D, AREATEX_WIDTH, AREATEX_HEIGHT, 1, 1, 1, GL_RG8, RSX_FORMAT_CLASS_COLOR);
		m_search_tex = std::make_unique<gl::texture>(GL_TEXTURE_2D, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1, 1, 1, GL_R8, RSX_FORMAT_CLASS_COLOR);
		std::vector<unsigned char> areaTexBytes_flipped = flipVertically(areaTexBytes, AREATEX_WIDTH, AREATEX_HEIGHT, 2);
		std::vector<unsigned char> searchTexBytes_flipped = flipVertically(searchTexBytes, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1);
		printf("Initialized SMAA Textures\n");
		glActiveTexture(0);
		GLint originalAlignment;
		glGetIntegerv(GL_UNPACK_ROW_LENGTH, &originalAlignment);
		glBindTexture(GL_TEXTURE_2D, m_area_tex->id());
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RG, GL_UNSIGNED_BYTE, areaTexBytes);
		glBindTexture(GL_TEXTURE_2D, m_search_tex->id());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, searchTexBytes);
		glBindTexture(GL_TEXTURE_2D, GL_NONE);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, originalAlignment);
		printf("Uploaded SMAA Textures\n");
	}


	void smaa_pass::replaceInclude(std::string& shader_source, std::string include_name,
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

	void smaa_pass::reset_sampler_states() {
		for (auto& sampler_state : saved_sampler_states)
		{
			sampler_state.reset();
		}
	}

	void smaa_pass::save_blend_state() {
		glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
		glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
	}

	void smaa_pass::load_blend_state() {
		glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
	}
	gl::texture* smaa_pass::antialias_output(gl::command_context& cmd, gl::texture* src, const areai& src_region)
	{
		// TODO: Implement Texture Reallocation on Resolution Scale Change (or if resolution higher than preallocated texture)

		// Bind Framebuffer and VAO
		m_vao.bind();
		m_fbo.bind();

		
		// Create Src Texture with stripped transparency
		m_fbo.color = m_intermediate_texture[4]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, src->id());
		cmd->use_program(m_program[3].id());
		attachUniforms(m_program[3].id());
		glUniform1i(uniform_locs.convert_colors, 0);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		reset_sampler_states();

		// Create Linearized "SMAA_INPUT" Texture for Neighborhood Blending Pass 
		m_fbo.color = m_intermediate_texture[3]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, src->id());
		cmd->use_program(m_program[3].id());
		attachUniforms(m_program[3].id());
		glUniform1i(uniform_locs.convert_colors, 1);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		glDisable(GL_BLEND);

		// Create Edge Detection Texture
		m_fbo.color = m_intermediate_texture[0]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[4]->id());
		cmd->use_program(m_program[0].id());
		attachUniforms(m_program[0].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		// Create Blending Weight Calculation Texture
		m_fbo.color = m_intermediate_texture[1]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 0));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[0]->id());
		saved_sampler_states[1] = std::make_unique<saved_sampler_state>(1, m_sampler[1]);
		cmd->bind_texture(1, GL_TEXTURE_2D, m_area_tex->id());
		saved_sampler_states[2] = std::make_unique<saved_sampler_state>(2, m_sampler[1]);
		cmd->bind_texture(2, GL_TEXTURE_2D, m_search_tex->id());
		cmd->use_program(m_program[1].id());
		attachUniforms(m_program[1].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		glEnable(GL_BLEND);
		//save_blend_state();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Create Neighborhood Blending Texture
		m_fbo.color = m_intermediate_texture[2]->id();
		m_fbo.read_buffer(m_fbo.color);
		m_fbo.draw_buffer(m_fbo.color);
		glViewport(0, 0, src_region.width(), src_region.height());
		cmd->clear_color(color4f(0, 0, 0, 1));
		glClear(GL_COLOR_BUFFER_BIT);
		saved_sampler_states[0] = std::make_unique<saved_sampler_state>(0, m_sampler[1]);
		cmd->bind_texture(0, GL_TEXTURE_2D, m_intermediate_texture[1]->id());
		saved_sampler_states[1] = std::make_unique<saved_sampler_state>(1, m_sampler[1]);
		cmd->bind_texture(1, GL_TEXTURE_2D, m_intermediate_texture[3]->id());
		cmd->use_program(m_program[2].id());
		attachUniforms(m_program[2].id());
		glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		glUniform1i(uniform_locs.convert_colors, 2);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices.data());
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		reset_sampler_states();

		//load_blend_state();
		glDisable(GL_BLEND);

		return m_intermediate_texture[2].get();
	}
} // namespace gl
