#pragma once

#include "../antialiasing.h"

namespace vk
{
	class fxaa_pass : public antialiasing_filter
	{
	struct push_constant_uniform
	{
		std::array<float, 4> i_resolution{};
		int convert_colors;
	};
	struct ScreenRectVertex
	{
		ScreenRectVertex() = default;
		ScreenRectVertex(float x, float y, float u, float v)
		{
			position[0] = x;
			position[1] = y;
			tex_coord[0] = u;
			tex_coord[1] = v;
		}

		std::array<float, 2> position{};
		std::array<float, 2> tex_coord{};
	};
	public:
		fxaa_pass();
		~fxaa_pass();

		vk::viewable_image* antialias_output(
			const vk::command_buffer& cmd,       // CB
			vk::viewable_image* src              // Source input
			) override;

	private:
		push_constant_uniform uniforms;
		VkRenderPass m_texture_renderpass;
		std::unique_ptr<vk::sampler> m_sampler;
		std::unique_ptr<vk::viewable_image> m_intermediate_texture;
		std::unique_ptr<vk::framebuffer> m_intermediate_texture_fbo;
		std::unique_ptr<vk::buffer> m_vbo;
		vk::glsl::shader m_vert_shader;
		vk::glsl::shader m_frag_shader;

		std::unique_ptr<vk::glsl::program> m_program;
		areai src_region = {0, 0, 1, 1};
		areai prev_src_region = {0, 0, 1, 1};
		bool textureInit;
		void allocateTextures(const vk::command_buffer& cmd, areai internal_res);
		void allocateTexture(const vk::command_buffer& cmd, int width, int height, VkImageLayout dst_layout, std::unique_ptr<vk::viewable_image>& texture);
		void allocateFBO(int width, int height, std::unique_ptr<vk::viewable_image>& texture, std::unique_ptr<vk::framebuffer>& framebuffer);
		void compileShaderProgram(vk::glsl::shader vs, vk::glsl::shader fs, std::unique_ptr<vk::glsl::program>& shader_program, bool enableBlend);
		void setViewportAndScissor(const vk::command_buffer& cmd, coordu region);
		std::array<ScreenRectVertex, 4> m_vertices;
	
	};
} // namespace gl
