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

namespace vk
{
	struct antialiasing_filter
	{
		virtual ~antialiasing_filter() {}

		virtual vk::viewable_image* antialias_output(
			const vk::command_buffer& cmd,        // CB
			vk::viewable_image* src               // Source input
			) = 0;
	};
} // namespace vk
