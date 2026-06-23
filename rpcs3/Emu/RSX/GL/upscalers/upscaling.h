#pragma once

#include "../glutils/image.h"
#include "../glutils/state_tracker.hpp"
#include "../glutils/buffer_object.h"
#include "../glutils/vao.hpp"
#include "../glutils/fbo.h"
#include "../glutils/sampler.h"
#include "../glutils/program.h"

namespace gl
{
	namespace upscaling_flags_
	{
		enum upscaling_flags
		{
			UPSCALE_DEFAULT_VIEW = (1 << 0),
			UPSCALE_LEFT_VIEW = (1 << 0),
			UPSCALE_RIGHT_VIEW = (1 << 1),
			UPSCALE_AND_COMMIT = (1 << 2)
		};
	}

	using namespace upscaling_flags_;

	struct upscaler
	{
		virtual ~upscaler() {}

		virtual gl::texture* scale_output(
			gl::command_context& cmd,               // State
			gl::texture* src,                       // Source input
			const areai& src_region,                // Scaling request information
			const areai& dst_region,                // Ditto
			gl::flags32_t mode                      // Mode
		) = 0;
	};
}
