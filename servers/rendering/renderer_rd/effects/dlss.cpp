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
	// Initialize motion vector decode shader
	{
		Vector<String> modes;
		modes.push_back("\n");
		shaders.mvec_decode_shader.initialize(modes, "");
		shaders.mvec_decode_version = shaders.mvec_decode_shader.version_create();
		shaders.mvec_decode_pipeline = RD::get_singleton()->compute_pipeline_create(shaders.mvec_decode_shader.version_get_shader(shaders.mvec_decode_version, 0));
	}
}

DLSSEffect::~DLSSEffect() {
	// Deinitialize motion vector decode
	{
		shaders.mvec_decode_shader.version_free(shaders.mvec_decode_version);
	}
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

static sl::float4x4 sl_make_identity_matrix()
{
	sl::float4x4 ret;
	ret.setRow(0, sl::float4(1.0f, 0.0f, 0.0f, 0.0f));
	ret.setRow(1, sl::float4(0.0f, 1.0f, 0.0f, 0.0f));
	ret.setRow(2, sl::float4(0.0f, 0.0f, 1.0f, 0.0f));
	ret.setRow(3, sl::float4(0.0f, 0.0f, 0.0f, 1.0f));
	return ret;
}

static sl::float4x4 sl_convert_matrix(const Projection& mtx) {
	sl::float4x4 ret;
	ret.setRow(0, sl::float4(mtx.columns[0].x, mtx.columns[1].x, mtx.columns[2].x, mtx.columns[3].x));
	ret.setRow(1, sl::float4(mtx.columns[0].y, mtx.columns[1].y, mtx.columns[2].y, mtx.columns[3].y));
	ret.setRow(2, sl::float4(mtx.columns[0].z, mtx.columns[1].z, mtx.columns[2].z, mtx.columns[3].z));
	ret.setRow(3, sl::float4(mtx.columns[0].w, mtx.columns[1].w, mtx.columns[2].w, mtx.columns[3].w));
	return ret;
}

static sl::float3 sl_convert_vector(const Vector3& vec) {
	return sl::float3(vec.x, vec.y, vec.z);
}

void DLSSEffect::upscale(const Parameters &p_params) {
	DLSSContextInner* context = (DLSSContextInner*)p_params.context;

	if(StreamlineContext::get().slDLSSSetOptions == nullptr)
		return;

	// Begin frame if needed
	if (StreamlineContext::get().last_token == nullptr)
		StreamlineContext::get().get_frame_token();

	// Set DLSS options
	if(p_params.exposure.is_null() || p_params.exposure.is_valid() == false)
		context->currentDlssOptions.useAutoExposure = sl::Boolean::eTrue;
	else
		context->currentDlssOptions.useAutoExposure = sl::Boolean::eFalse;

	context->currentDlssOptions.colorBuffersHDR = sl::Boolean::eTrue;

	StreamlineContext::get().slDLSSSetOptions(context->viewport, context->currentDlssOptions);

	// Decode mvecs
	{
		UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
		ERR_FAIL_NULL(uniform_set_cache);
		MaterialStorage *material_storage = MaterialStorage::get_singleton();
		ERR_FAIL_NULL(material_storage);

		// setup our uniforms
		RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

		RD::Uniform u_velocity_image(RD::UNIFORM_TYPE_IMAGE, 0, p_params.velocity);
		RD::Uniform u_depth_texture(RD::UNIFORM_TYPE_TEXTURE, 0, p_params.depth);

		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

		RID shader = shaders.mvec_decode_shader.version_get_shader(shaders.mvec_decode_version, 0);
		ERR_FAIL_COND(shader.is_null());

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, shaders.mvec_decode_pipeline);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_velocity_image), 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_depth_texture), 1);

		auto texture_format = RD::get_singleton()->texture_get_format(p_params.velocity);

		float push_constants[20];
		push_constants[0] = texture_format.width;
		push_constants[1] = texture_format.height;
		push_constants[2] = 0.0f;
		push_constants[3] = 0.0f;
		memcpy(push_constants+4, &p_params.reprojection.columns[0].x, sizeof(float)*16);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, push_constants, sizeof(push_constants));

		RD::get_singleton()->compute_list_dispatch_threads(compute_list, texture_format.width, texture_format.height, 1);
		RD::get_singleton()->compute_list_add_barrier(compute_list);

		RD::get_singleton()->compute_list_end();
	}

	// Set SL Options
	{
		sl::float4x4 mtxIdentity = sl_make_identity_matrix();
		context->constants.cameraViewToClip = sl_convert_matrix(p_params.cam_projection); // projection mtx (unjittered)
		context->constants.clipToCameraView = sl_convert_matrix(p_params.cam_projection.inverse()); // projection mtx (unjittered, inverted)
		context->constants.clipToLensClip = mtxIdentity; // keep identity unless some lens distortion is applied
		context->constants.clipToPrevClip = sl_convert_matrix(p_params.reprojection); // reprojection matrix
		context->constants.prevClipToClip = sl_convert_matrix(p_params.reprojection.inverse()); // inverted reprojection matrix

		context->constants.cameraPos = sl_convert_vector(p_params.cam_transform.get_origin());
		context->constants.cameraFwd = sl_convert_vector(p_params.cam_transform.get_basis().rows[0]);
		context->constants.cameraUp = sl_convert_vector(p_params.cam_transform.get_basis().rows[1]);
		context->constants.cameraRight = sl_convert_vector(p_params.cam_transform.get_basis().rows[2]);

		context->constants.cameraNear = p_params.z_near;
		context->constants.cameraFar = p_params.z_far;
		context->constants.cameraFOV = p_params.fovy;
		context->constants.cameraMotionIncluded = sl::Boolean::eTrue;
		context->constants.cameraAspectRatio = context->currentDlssOptions.outputWidth / context->currentDlssOptions.outputHeight;
		context->constants.cameraPinholeOffset = sl::float2(0.0f, 0.0f);
		context->constants.depthInverted = sl::Boolean::eTrue;
		context->constants.motionVectors3D = sl::Boolean::eFalse;
		context->constants.motionVectorsDilated = sl::Boolean::eFalse;
		context->constants.motionVectorsJittered = sl::Boolean::eFalse;
		context->constants.jitterOffset = sl::float2(p_params.jitter.x, p_params.jitter.y);
		context->constants.mvecScale = sl::float2(1.0f, 1.0f);
		context->constants.orthographicProjection = sl::Boolean::eFalse;
		context->constants.reset = p_params.reset_accumulation ? sl::Boolean::eTrue : sl::Boolean::eFalse;
		if(StreamlineContext::get().slSetConstants(context->constants, *StreamlineContext::get().last_token, context->viewport) != sl::Result::eOk)
			ERR_FAIL_MSG("Failed to call streamline slSetConstants.");
	}

	// Tag resources
	{
        sl::Resource resources[6];
        sl::ResourceTag resourceTags[6];
        int numResources = 0;

        auto assignResource = [this, &resources, &resourceTags, &numResources](RID textureRID, sl::BufferType bufferType, sl::ResourceLifecycle lifecycle) {
            if (textureRID.is_valid() == false || textureRID.is_null() == true)
				return;	

			RD::TextureFormat texture_format = RD::get_singleton()->texture_get_format(textureRID);
			uint64_t texture_image = RD::get_singleton()->get_driver_resource(RD::DriverResource::DRIVER_RESOURCE_VULKAN_IMAGE, textureRID);
			uint64_t texture_view = RD::get_singleton()->get_driver_resource(RD::DriverResource::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW, textureRID);
			uint64_t texture_device_memory = RD::get_singleton()->get_driver_resource(RD::DriverResource::DRIVER_RESOURCE_VULKAN_IMAGE_DEVICE_MEMORY, textureRID);;
			uint64_t texture_state = RD::get_singleton()->get_driver_resource(RD::DriverResource::DRIVER_RESOURCE_VULKAN_IMAGE_LAYOUT, textureRID);
			uint64_t texture_vkformat = RD::get_singleton()->get_driver_resource(RD::DriverResource::DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT, textureRID);
			uint64_t texture_usage_flags = RD::get_singleton()->get_driver_resource(RD::DriverResource::DRIVER_RESOURCE_VULKAN_IMAGE_USAGE_FLAGS, textureRID);
			auto& destinationResource = resources[numResources];
			destinationResource = sl::Resource(sl::ResourceType::eTex2d,
				(void*)texture_image, (void*)texture_device_memory, (void*)texture_view, texture_state);
			destinationResource.width = texture_format.width;
			destinationResource.height = texture_format.height;
			destinationResource.nativeFormat = texture_vkformat;
			destinationResource.arrayLayers = texture_format.array_layers;
			destinationResource.flags = 0;
			destinationResource.mipLevels = texture_format.mipmaps;
			destinationResource.usage = texture_usage_flags;

			resourceTags[numResources] = sl::ResourceTag(resources + numResources, bufferType, lifecycle, nullptr);
			++numResources;
        };
        assignResource(p_params.color, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent);
        assignResource(p_params.output, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent);
        assignResource(p_params.depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent);
        assignResource(p_params.velocity, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent);
        assignResource(p_params.exposure, sl::kBufferTypeExposure, sl::ResourceLifecycle::eValidUntilPresent);

        if (StreamlineContext::get().slSetTag(context->viewport, resourceTags, numResources, nullptr) != sl::Result::eOk)
			ERR_FAIL_MSG("Failed to call streamline slSetTag.");
	}

	// Evaluate DLSS Super Res
	{
		const sl::BaseStructure *inputs[] = { &context->viewport };
		void *nativeCmdlist = (void*)RD::get_singleton()->get_driver_resource(RenderingDevice::DriverResource::DRIVER_RESOURCE_VULKAN_COMMAND_BUFFER_DRAW);
		auto dlssResult = StreamlineContext::get().slEvaluateFeature(sl::kFeatureDLSS, *StreamlineContext::get().last_token, inputs, 1, nativeCmdlist);
		if (dlssResult != sl::Result::eOk)
			ERR_FAIL_MSG("Failed to call streamline slEvaluateFeature for DLSS Super Resolution.");
	}
}
#else
DLSSContext::~DLSSContext() {}
DLSSContext::DLSSContext() {}
DLSSEffect::DLSSEffect() {}
DLSSEffect::~DLSSEffect() {}
DLSSContext *DLSSEffect::create_context(Size2i p_internal_size, Size2i p_target_size) {}
void DLSSEffect::upscale(const Parameters &p_params) {}
#endif
