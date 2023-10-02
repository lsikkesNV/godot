/**************************************************************************/
/*  dlss.h                                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef DLSS_RD_H
#define DLSS_RD_H

#include "servers/rendering/renderer_rd/shader_rd.h"

#include "servers/rendering/renderer_rd/shaders/effects/motion_vector_decode.glsl.gen.h"

namespace RendererRD {

class DLSSContext {
public:
	virtual ~DLSSContext() {}
};

class DLSSEffect {
public:
	struct Device {

	};

	struct Shaders {
		MotionVectorDecodeShaderRD mvec_decode_shader;
		RID mvec_decode_version;
		RID mvec_decode_pipeline;
	} shaders;

	struct Parameters {
		DLSSContext *context;
		Size2i internal_size;
		RID color;
		RID depth;
		RID velocity;
		RID reactive;
		RID exposure;
		RID output;
		float z_near = 0.0f;
		float z_far = 0.0f;
		float fovy = 0.0f;
		Vector2 jitter;
		float delta_time = 0.0f;
		float sharpness = 0.0f;
		bool reset_accumulation = false;
		Projection reprojection;
		Projection cam_projection;
		Transform3D cam_transform;
	};

	DLSSEffect();
	~DLSSEffect();
	DLSSContext *create_context(Size2i p_internal_size, Size2i p_target_size);
	void upscale(const Parameters &p_params);
	bool is_ready(DLSSContext* context);
private:
	Device device;
};

} // namespace RendererRD

#endif // DLSS_RD_H
