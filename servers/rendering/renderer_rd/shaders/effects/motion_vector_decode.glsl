///////////////////////////////////////////////////////////////////////////////////
// Copyright(c) 2016-2022 Panos Karabelas
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////
// File changes (yyyy-mm-dd)
// 2022-05-06: Panos Karabelas: first commit
// 2020-12-05: Joan Fons: convert to Vulkan and Godot
///////////////////////////////////////////////////////////////////////////////////

#[compute]

#version 450

#VERSION_DEFINES

#extension GL_EXT_samplerless_texture_functions : require

#include "motion_vector_inc.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rg16f, set = 0, binding = 0) uniform restrict image2D velocity_buffer;
layout (set = 1, binding = 0) uniform texture2D depth_buffer;

layout(push_constant, std430) uniform Params {
	vec4 resolution;
	mat4 reprojection;
}
params;


void main() {
	const ivec2 pos_screen = ivec2(gl_GlobalInvocationID.xy);

	vec2 velocity =  imageLoad(velocity_buffer, pos_screen).rg;

	bool invalid_motion_vector = all(lessThanEqual(velocity, vec2(-1.0f, -1.0f)));
	if (invalid_motion_vector)
	{
		float src_depth = texelFetch(depth_buffer, pos_screen, 0).r;
		vec2 uv = (vec2(pos_screen) + float(0.5)) / params.resolution.xy;
		velocity = derive_motion_vector(uv, src_depth, params.reprojection);
	}

	imageStore(velocity_buffer, pos_screen, vec4(velocity, 0.0f, 0.0f));
}
