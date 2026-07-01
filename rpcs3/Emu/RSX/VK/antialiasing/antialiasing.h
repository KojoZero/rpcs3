#pragma once


#include "../vkutils/commands.h"
#include "../vkutils/image.h"
#include "../vkutils/sampler.h"
#include "../vkutils/device.h"
#include "../vkutils/framebuffer_object.hpp"
#include "../VKHelpers.h"
#include "../VKProgramPipeline.h"
#include "../VKPipelineCompiler.h"
#include "../VKRenderPass.h"
//#include "../glutils/image.h"
//#include "../glutils/state_tracker.hpp"
//#include "../glutils/buffer_object.h"
//#include "../glutils/vao.hpp"
//#include "../glutils/fbo.h"
//#include "../glutils/sampler.h"
//#include "../glutils/program.h"
namespace vk
{
	struct antialiasing_filter
	{
		virtual ~antialiasing_filter() {}

		virtual vk::viewable_image* antialias_output(
			const vk::command_buffer& cmd,        // CB
			vk::viewable_image* src,              // Source input
			VkImage present_surface,              // Present target. May be VK_NULL_HANDLE for some passes
			VkImageLayout present_surface_layout // Present surface layout, or VK_IMAGE_LAYOUT_UNDEFINED if no present target is provided
			) = 0;
	};
} // namespace vk
