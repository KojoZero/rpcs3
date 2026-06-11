#pragma once

#include "../glutils/image.h"
#include "../glutils/state_tracker.hpp"

namespace gl
{
	struct antialiasing_filter
	{
		virtual ~antialiasing_filter() {}

		virtual gl::texture* scale_output(
			gl::command_context& cmd, // State
			gl::texture* src,         // Source input
			const areai& src_region,  // Scaling request information
			const areai& dst_region,  // Ditto
			gl::flags32_t mode        // Mode
			) = 0;
	};
} // namespace gl
