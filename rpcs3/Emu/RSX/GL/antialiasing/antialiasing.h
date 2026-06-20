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
	struct antialiasing_filter
	{
		virtual ~antialiasing_filter() {}

		virtual gl::texture* antialias_output(
			gl::command_context& cmd, // State
			gl::texture* src,         // Source input
			const areai& src_region  // Scaling request information
			) = 0;
	};
} // namespace gl
