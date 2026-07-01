#include "fxaa_pass.h"
#include "fxaa_shader.h"
namespace vk
{
	fxaa_pass::fxaa_pass() {
		m_vertices = {
			ScreenRectVertex(-1.f, 1.f, 0.f, 1.f),  // Left,  Top
			ScreenRectVertex(1.f, 1.f, 1.f, 1.f),   // Right, Top
			ScreenRectVertex(-1.f, -1.f, 0.f, 0.f), // Left,  Bottom
			ScreenRectVertex(1.f, -1.f, 1.f, 0.f),  // Right, Bottom
		};
		const auto pdev = vk::get_current_renderer();
		m_sampler = std::make_unique<vk::sampler>(*pdev,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_FALSE, 0.f, 1.f, 0.f, 0.f, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK);
		m_vert_shader.create(::glsl::program_domain::glsl_vertex_program, FXAA_VERT);
		m_vert_shader.compile();
		m_frag_shader.create(::glsl::program_domain::glsl_fragment_program, FXAA_FRAG);
		m_frag_shader.compile();
		compileShaderProgram(m_vert_shader, m_frag_shader, m_program, false);
		m_texture_renderpass = vk::get_renderpass(*pdev, vk::get_renderpass_key(VK_FORMAT_R16G16B16A16_SFLOAT));
		//m_vbo.create(sizeof(ScreenRectVertex) * 4, m_vertices.data(), gl::buffer::memory_type::local, 0);
		//m_vbo.bind();
		//m_fbo.create();
	}

	void fxaa_pass::compileShaderProgram(vk::glsl::shader vs, vk::glsl::shader fs, std::unique_ptr<vk::glsl::program>& shader_program, bool enableBlend)
	{
		// Configuring Pipeline State
		VkPipelineInputAssemblyStateCreateInfo ia{};
		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

		VkPipelineDepthStencilStateCreateInfo ds{};
		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.depthTestEnable = VK_FALSE;
		ds.depthWriteEnable = VK_FALSE;
		ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;

		VkPipelineColorBlendAttachmentState att_state{};
		att_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		                           VK_COLOR_COMPONENT_G_BIT |
		                           VK_COLOR_COMPONENT_B_BIT |
		                           VK_COLOR_COMPONENT_A_BIT;
		att_state.blendEnable = enableBlend == true ? VK_TRUE : VK_FALSE;
		att_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		att_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		att_state.colorBlendOp = VK_BLEND_OP_ADD;
		att_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		att_state.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo cs{};
		cs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cs.logicOpEnable = VK_FALSE;
		cs.attachmentCount = 1;
		cs.pAttachments = &att_state;

		VkPipelineRasterizationStateCreateInfo rs{};
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.cullMode = VK_CULL_MODE_NONE;
		rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo ms{};
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		ms.minSampleShading = 1.0f;

		VkVertexInputBindingDescription vertex_binding = {
			.binding = 0,
			.stride = sizeof(ScreenRectVertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};

		VkVertexInputAttributeDescription vertex_attributes[2] = {
			{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(ScreenRectVertex, position),
			},
			{
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(ScreenRectVertex, tex_coord),
			},
		};

		VkPipelineVertexInputStateCreateInfo vi = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertex_binding,
			.vertexAttributeDescriptionCount = 2,
			.pVertexAttributeDescriptions = vertex_attributes,
		};

		// Setup Pipeline State and Get Renderpass Key
		graphics_pipeline_state create_info_pipeline_state;
		create_info_pipeline_state.ia = ia;
		create_info_pipeline_state.ds = ds;
		create_info_pipeline_state.att_state[0] = att_state;
		create_info_pipeline_state.cs = cs;
		create_info_pipeline_state.rs = rs;
		create_info_pipeline_state.ms = ms;
		create_info_pipeline_state.vi = vi;
		
		u64 create_info_renderpass_key = vk::get_renderpass_key(VK_FORMAT_R16G16B16A16_SFLOAT);

		// Create Props and start Compiling
		pipeline_props create_info_props =
		{
			.state = create_info_pipeline_state,
			.renderpass_key = create_info_renderpass_key
		};

		// Setup Inputs
		std::vector<vk::glsl::program_input> vertex_inputs =
		{
			glsl::program_input::make(
				::glsl::program_domain::glsl_vertex_program,
				"vertex_push_constants",
				glsl::program_input_type::input_type_push_constant,
				0,
				0,
				glsl::push_constant_ref{.offset = 0, .size = sizeof(push_constant_uniform)})
		};

		std::vector<vk::glsl::program_input> fragment_inputs =
		{
			glsl::program_input::make(
				::glsl::program_domain::glsl_fragment_program,
				"color_texture",
				vk::glsl::input_type_texture,
				0,
				0),
			glsl::program_input::make(
				::glsl::program_domain::glsl_fragment_program,
				"fragment_push_constants",
				glsl::program_input_type::input_type_push_constant,
				0,
				0,
				glsl::push_constant_ref{.offset = sizeof(push_constant_uniform), .size = sizeof(push_constant_uniform)})
		};
		

		auto compiler = vk::get_pipe_compiler();
		shader_program = compiler->compile(create_info_props, vs.get_handle(), fs.get_handle(), vk::pipe_compiler::COMPILE_INLINE, {}, vertex_inputs, fragment_inputs);
	}

	fxaa_pass::~fxaa_pass() {
		//printf("Destroying: FXAA PASS\n");
		//m_vao.remove();
		//m_vbo.remove();
		//m_vert_shader.remove();
		//m_frag_shader.remove();
		//m_program.remove();
		//m_fbo.remove();
		//m_sampler.remove();
		//m_intermediate_texture.reset();
		//printf("Destroyed: FXAA PASS\n");
	}

	void fxaa_pass::allocateTextures(const vk::command_buffer& cmd, areai internal_res)
	{
		m_intermediate_texture.reset();
		allocateTexture(cmd, internal_res.width(), internal_res.height(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, m_intermediate_texture);
		allocateFBO(internal_res.width(), internal_res.height(), m_intermediate_texture, m_intermediate_texture_fbo);
	}

	void fxaa_pass::allocateTexture(const vk::command_buffer& cmd, int width, int height, VkImageLayout dst_layout, std::unique_ptr<vk::viewable_image>& texture)
	{
		const auto pdev = vk::get_current_renderer();
		VkFlags usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		texture = std::make_unique<vk::viewable_image>(*pdev, pdev->get_memory_mapping().device_local, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, width, height, 1, 1, 1, VK_SAMPLE_COUNT_1_BIT, //  (image Type (dimensions), texture format, width, height, depth, mips, layers, samples)
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, usage_flags,
			VK_IMAGE_CREATE_ALLOW_NULL_RPCS3, VMM_ALLOCATION_POOL_SWAPCHAIN, RSX_FORMAT_CLASS_COLOR);
		texture->change_layout(cmd, dst_layout);
		texture->get_view(rsx::default_remap_vector, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void fxaa_pass::allocateFBO(int width, int height, std::unique_ptr<vk::viewable_image>& texture, std::unique_ptr<vk::framebuffer>& framebuffer)
	{
		const auto pdev = vk::get_current_renderer();
		framebuffer = std::make_unique<vk::framebuffer>(*pdev, m_texture_renderpass, width, height, texture->get_view(rsx::default_remap_vector, VK_IMAGE_ASPECT_COLOR_BIT));
	}

	vk::viewable_image* fxaa_pass::antialias_output(
		const vk::command_buffer& cmd,       // CB
		vk::viewable_image* src,             // Source input
		VkImage present_surface,             // Present target. May be VK_NULL_HANDLE for some passes
		VkImageLayout present_surface_layout // Present surface layout, or VK_IMAGE_LAYOUT_UNDEFINED if no present target is provided
	) {
		// Add texture init for case where src_region is 0,0,1,1 or set prev_src_region to some impossible number
		src_region = {0, 0, src->width(), src->height()};
		if (src_region.width() != prev_src_region.width() || src_region.height() != prev_src_region.height())
		{
			allocateTextures(cmd, src_region);
			prev_src_region = src_region;
		}

		//// Bind Framebuffer and VAO
		//glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
		//m_vao.bind();
		//m_fbo.bind();

		//// Start Antialiasing
		//m_fbo.color = m_intermediate_texture->id();
		//m_fbo.read_buffer(m_fbo.color);
		//m_fbo.draw_buffer(m_fbo.color);
		//glViewport(0, 0, src_region.width(), src_region.height());
		//cmd->clear_color(color4f(0,0,0,1));
		//glClear(GL_COLOR_BUFFER_BIT);
		//saved_sampler_state saved(0, m_sampler);
		//cmd->bind_texture(0, GL_TEXTURE_2D, src->id());
		//cmd->use_program(m_program.id());
		//attachUniforms(m_program.id());
		//glUniform4f(uniform_locs.i_resolution, src_region.width(), src_region.height(), 1.0f / src_region.width(), 1.0f / src_region.height());
		//glUniform1i(uniform_locs.convert_colors, 0);
		//glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glBindVertexArray(prev_vao);




		// Vulkan Single Pass
		m_program->bind_uniform({*m_intermediate_texture->get_view(rsx::default_remap_vector, VK_IMAGE_ASPECT_COLOR_BIT), *m_sampler}, 0, 0);
		vkCmdPushConstants(cmd, m_program->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constant_uniform), &uniforms);
		vkCmdPushConstants(cmd, m_program->layout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(push_constant_uniform), sizeof(push_constant_uniform), &uniforms);


		return m_intermediate_texture.get();
	}
} // namespace gl
