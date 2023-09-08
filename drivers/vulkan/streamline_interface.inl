#pragma once

namespace streamline {
struct float3
{
	float v[3];
};
struct matrix4x4
{
	float m[16];
};

struct gfx_extension_nvapi_dlss_query {
	enum class mode {
		Off = 0,
		MaxPerformance,
		Balanced,
		MaxQuality,
		UltraPerformance,
		UltraQuality,
		DLAA,
	};
	mode input_DLSSMode = mode::Off;
	int input_OutputResolutionX = -1;
	int input_OutputResolutionY = -1;

	int output_ViewportResolutionX = -1;
	int output_ViewportResolutionY = -1;
	float output_OptimalSharpness = -1.0f;
	bool output_Valid = false;
};

struct gfx_extension_nvapi_dlss_tag {
	int input_viewportID = 0;
	usize input_textureColorInput = cfc::invalid_index;
	usize input_textureColorOutput = cfc::invalid_index;
	usize input_textureDepth = cfc::invalid_index;
	usize input_textureMotionVectors = cfc::invalid_index;
	usize input_textureExposure = cfc::invalid_index;

	bool output_Valid = false;
};

struct gfx_extension_nvapi_dlss_evaluate {
	int input_viewportID = 0;
	usize input_commandList = cfc::invalid_index;

	bool output_Valid = false;
};

struct gfx_extension_nvapi_dlss_constants {
	gfx_extension_nvapi_dlss_query::mode mode = gfx_extension_nvapi_dlss_query::mode::Off;
	int viewportID = 0;
	int outputResolutionX = -1;
	int outputResolutionY = -1;
	float sharpness = -1.0f; // negative = use optimal sharpness
	bool isHDRInput = false;
	bool useAutoExposure = true;
};

struct gfx_extension_nvapi_sl_constants {
	int viewportID = 0;

	float mvecScaleX = 1.0f;
	float mvecScaleY = 1.0f;
	float jitterOffsetX = 0.0f;
	float jitterOffsetY = 0.0f;

	bool reset = false;
	bool isOrthoMatrix = false;
	bool motionVectors_with_camera_motion = true;
	bool motionVectors_dilated = false;
	bool motionVectors_jittered = false;
	bool motionVectors_3d = false;
	float motionVector_invalidValue = FLT_MAX;

	// camera
	bool depthInverted = true;
	float3 cameraPosition;
	float3 cameraDirection;
	float3 cameraUp;
	float3 cameraRight;
	float3 cameraPinholeOffset;
	float cameraNear;
	float cameraFar;
	float cameraAspectRatio; // width / height
	float cameraFOV; // in radians

	// unjittered matrices
	matrix4x4 projection;
	matrix4x4 projectionInverse;

	/*
	*   Example:
		float4x4 viewReprojection = GetInverseViewMatrix() * previous->GetViewMatrix();
		float4x4 reprojectionMatrix = inverse(GetUnjitteredProjectionMatrix()) * viewReprojection * previous->GetUnjitteredProjectionMatrix();
		clipToPrevClip = reprojectionMatrix;
		prevClipToClip = inverse(reprojectionMatrix);
	*/
	matrix4x4 clipToPrevClip;
	matrix4x4 clipToPrevClipInverse;
};

}; //namespace streamline
