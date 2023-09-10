/**************************************************************************/
/*  dlss.cpp                                                              */
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

#include "dlss.h"

#include "../storage_rd/material_storage.h"
#include "../uniform_set_cache_rd.h"

#define USE_STREAMLINE 1

#ifdef USE_STREAMLINE
#define ENABLE_DLSS
#endif

#ifdef ENABLE_DLSS
#include "../../../../drivers/vulkan/vulkan_context_streamline.inl"
#endif

using namespace RendererRD;

#ifdef ENABLE_DLSS
namespace RendererRD {
class DLSSContextInner : public DLSSContext {
public:
	sl::ViewportHandle viewport;
	sl::Constants constants = {};
	sl::DLSSOptions currentDlssOptions = {};
	sl::DLSSOptimalSettings currentOptimalSettings = {};

	DLSSContextInner();
	virtual ~DLSSContextInner();

	sl::DLSSMode find_optimal_mode(uint32_t outputWidth, uint32_t outputHeight, uint32_t desiredWidth, uint32_t desiredHeight, sl::DLSSOptimalSettings& optimalSettings) {
		if(StreamlineContext::get().slDLSSGetOptimalSettings == nullptr)
			return sl::DLSSMode::eOff;

		sl::DLSSMode modes[] = { sl::DLSSMode::eDLAA, sl::DLSSMode::eMaxQuality, sl::DLSSMode::eBalanced, sl::DLSSMode::eMaxPerformance, sl::DLSSMode::eUltraPerformance };
		int selectedMode = -1;
		for(int i=0; i<sizeof(modes)/sizeof(modes[0]); i++)
		{
			sl::DLSSOptions dlssOptions = {};
			dlssOptions.outputWidth = outputWidth;
			dlssOptions.outputHeight = outputHeight;
			dlssOptions.mode = modes[i];

			sl::DLSSOptimalSettings optimalSettings = {};
			sl::Result result = StreamlineContext::get().slDLSSGetOptimalSettings(dlssOptions, optimalSettings);
			if (result != sl::Result::eOk)
				continue;

			if(	desiredWidth >= optimalSettings.renderWidthMin && 
				desiredWidth <= optimalSettings.renderWidthMax &&
				desiredHeight >= optimalSettings.renderHeightMin &&
				desiredHeight <= optimalSettings.renderHeightMax) {
				currentOptimalSettings = optimalSettings;
				return modes[i];
			}
		}
		return sl::DLSSMode::eOff; // invalid mode
	}
}; // end class
}; // end namespace RendererRD


DLSSContextInner::~DLSSContextInner() {
}

DLSSContextInner::DLSSContextInner() {
}

DLSSEffect::DLSSEffect() {
}

DLSSEffect::~DLSSEffect() {
}

DLSSContext *DLSSEffect::create_context(Size2i p_internal_size, Size2i p_target_size) {
	DLSSContextInner *context = memnew(RendererRD::DLSSContextInner);

	context->currentDlssOptions.mode = context->find_optimal_mode(p_target_size.width, p_target_size.height, p_internal_size.width, p_internal_size.height, context->currentOptimalSettings);
	context->currentDlssOptions.outputWidth = p_target_size.width;
	context->currentDlssOptions.outputHeight = p_target_size.height;
	context->currentDlssOptions.sharpness = context->currentOptimalSettings.optimalSharpness;

	if (true) {
		return context;
	} else {
		memdelete(context);
		return nullptr;
	}
}

void DLSSEffect::upscale(const Parameters &p_params) {
	DLSSContextInner* context = (DLSSContextInner*)p_params.context;
	
	if(StreamlineContext::get().slDLSSSetOptions == nullptr)
		return;

	// Set DLSS options
	if(p_params.exposure.is_null() || p_params.exposure.is_valid() == false)
		context->currentDlssOptions.useAutoExposure = sl::Boolean::eTrue;
	else
		context->currentDlssOptions.useAutoExposure = sl::Boolean::eFalse;	

	context->currentDlssOptions.colorBuffersHDR = sl::Boolean::eTrue;
	
	StreamlineContext::get().slDLSSSetOptions(context->viewport, context->currentDlssOptions);

	// Set SL Options
	context->constants.cameraNear = p_params.z_near;
	context->constants.cameraFar = p_params.z_far;
	context->constants.cameraFOV = 90.0f;
	context->constants.cameraMotionIncluded = sl::Boolean::eTrue;
	context->constants.cameraAspectRatio = context->currentDlssOptions.outputWidth / context->currentDlssOptions.outputHeight;
	context->constants.cameraPinholeOffset = sl::float2(0.0f, 0.0f);
	context->constants.depthInverted = sl::Boolean::eTrue;
	context->constants.motionVectors3D = sl::Boolean::eFalse;
	context->constants.motionVectorsDilated = sl::Boolean::eFalse;
	context->constants.motionVectorsJittered = sl::Boolean::eFalse;
	context->constants.motionVectorsInvalidValue = sl::Boolean::eFalse;
	context->constants.jitterOffset = sl::float2(p_params.jitter.x, p_params.jitter.y);
	context->constants.mvecScale = sl::float2(1.0f, 1.0f);
	context->constants.orthographicProjection = sl::Boolean::eFalse;
	context->constants.reset = p_params.reset_accumulation ? sl::Boolean::eTrue : sl::Boolean::eFalse;
	StreamlineContext::get().slSetConstants(context->constants, *StreamlineContext::get().last_token, context->viewport);
}
#else
DLSSContext::~DLSSContext() {}
DLSSContext::DLSSContext() {}
DLSSEffect::DLSSEffect() {}
DLSSEffect::~DLSSEffect() {}
DLSSContext *DLSSEffect::create_context(Size2i p_internal_size, Size2i p_target_size) {}
void DLSSEffect::upscale(const Parameters &p_params) {}
#endif
