#ifdef USE_STREAMLINE

#include "../../thirdparty/streamline/include/sl.h"
#include "../../thirdparty/streamline/include/sl_consts.h"
#include "../../thirdparty/streamline/include/sl_dlss.h"
#include "../../thirdparty/streamline/include/sl_dlss_g.h"
#include "../../thirdparty/streamline/include/sl_reflex.h"

class StreamlineContext
{
public:
	// Interposer
	PFun_slInit *slInit = nullptr;
	PFun_slShutdown *slShutdown = nullptr;
	PFun_slIsFeatureSupported *slIsFeatureSupported = nullptr;
	PFun_slGetFeatureFunction *slGetFeatureFunction = nullptr;
	PFun_slGetNewFrameToken *slGetNewFrameToken = nullptr;
	PFun_slAllocateResources *slAllocateResources = nullptr;
	PFun_slFreeResources *slFreeResources = nullptr;
	PFun_slEvaluateFeature *slEvaluateFeature = nullptr;
	PFun_slSetTag *slSetTag = nullptr;
	PFun_slSetConstants *slSetConstants = nullptr;

	// Reflex
	sl::ReflexOptions reflex_options;
	bool reflex_options_dirty = true;
	PFun_slReflexGetState* slReflexGetState = nullptr;
	PFun_slReflexSetOptions* slReflexSetOptions = nullptr;
	PFun_slReflexSetMarker* slReflexSetMarker = nullptr;
	PFun_slReflexSleep* slReflexSleep = nullptr;

	// DLSS Super Resolution
	PFun_slDLSSGetOptimalSettings *slDLSSGetOptimalSettings = nullptr;
	PFun_slDLSSGetState *slDLSSGetState = nullptr;
	PFun_slDLSSSetOptions *slDLSSSetOptions = nullptr;

	void load_functions();
	void load_functions_post_init();
	static const char* result_to_string(sl::Result result);
	void reflex_set_options(const sl::ReflexOptions& opts);
	void reflex_marker(sl::FrameToken* frameToken, sl::ReflexMarker marker);
	void reflex_sleep(sl::FrameToken* frameToken);
	void reflex_get_state(sl::ReflexState& reflexState);
	sl::FrameToken* get_frame_token();

	unsigned long long convert_rdformat_to_vkformat(RenderingDevice::DataFormat format);

	static StreamlineContext &get();

	sl::FrameToken* last_token = nullptr;
	bool isGame = false;

#ifdef STREAMLINE_IMPLEMENTATION
	VulkanContext::StreamlineCapabilities enumerate_support(VkPhysicalDevice device);
#endif
};

#ifdef STREAMLINE_IMPLEMENTATION

#include "vulkan_context.h"

StreamlineContext &StreamlineContext::get() {
	static StreamlineContext _context;
	return _context;
}

void StreamlineContext::load_functions() {
#ifdef _WIN32
	HMODULE streamline = LoadLibraryA("sl.interposer.dll");
	if(streamline == nullptr)
		return;
	this->slInit = (PFun_slInit *)GetProcAddress(streamline, "slInit");
	this->slShutdown = (PFun_slShutdown *)GetProcAddress(streamline, "slShutdown");
	this->slIsFeatureSupported = (PFun_slIsFeatureSupported *)GetProcAddress(streamline, "slIsFeatureSupported");
	this->slGetFeatureFunction = (PFun_slGetFeatureFunction *)GetProcAddress(streamline, "slGetFeatureFunction");
	this->slGetNewFrameToken = (PFun_slGetNewFrameToken *)GetProcAddress(streamline, "slGetNewFrameToken");

	this->slAllocateResources = (PFun_slAllocateResources *)GetProcAddress(streamline, "slAllocateResources");
	this->slFreeResources = (PFun_slFreeResources *)GetProcAddress(streamline, "slFreeResources");
	this->slEvaluateFeature = (PFun_slEvaluateFeature *)GetProcAddress(streamline, "slEvaluateFeature");
	this->slSetTag = (PFun_slSetTag *)GetProcAddress(streamline, "slSetTag");
	this->slSetConstants = (PFun_slSetConstants *)GetProcAddress(streamline, "slSetConstants");

#endif
}

void StreamlineContext::load_functions_post_init() {
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetOptions", (void*&)this->slReflexSetOptions);
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetMarker", (void*&)this->slReflexSetMarker);
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexSleep", (void*&)this->slReflexSleep);
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexGetState", (void*&)this->slReflexGetState);

	slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", (void*&)this->slDLSSGetOptimalSettings);
	slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGetState", (void*&)this->slDLSSGetState);
	slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSSetOptions", (void*&)this->slDLSSSetOptions);
}

VulkanContext::StreamlineCapabilities StreamlineContext::enumerate_support(VkPhysicalDevice device) {
	VulkanContext::StreamlineCapabilities support;
	sl::AdapterInfo adapterInfo;
	adapterInfo.vkPhysicalDevice = device;
	if(this->slIsFeatureSupported) {
		support.dlssAvailable = this->slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo) == sl::Result::eOk;
		support.dlssGAvailable = this->slIsFeatureSupported(sl::kFeatureDLSS_G, adapterInfo) == sl::Result::eOk;
		support.reflexAvailable = this->slIsFeatureSupported(sl::kFeatureReflex, adapterInfo) == sl::Result::eOk;
	}
	return support;
}

void StreamlineContext::reflex_set_options(const sl::ReflexOptions& opts) {
	if (isGame == false) // Disable reflex for editor or project settings.
		return;

	reflex_options = opts;
	reflex_options_dirty = false;
	sl::Result result = this->slReflexSetOptions ? this->slReflexSetOptions(opts) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

void StreamlineContext::reflex_marker(sl::FrameToken* frameToken, sl::ReflexMarker marker) {
	if (isGame == false) // Disable reflex for editor or project settings.
		return;

	sl::Result result = this->slReflexSetMarker ? this->slReflexSetMarker(marker, *frameToken) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

void StreamlineContext::reflex_sleep(sl::FrameToken* frameToken) {
	if (isGame == false) // Disable reflex for editor or project settings.
		return;

	if(frameToken == nullptr)
		return;
	sl::Result result = this->slReflexSleep ? this->slReflexSleep(*frameToken) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

void StreamlineContext::reflex_get_state(sl::ReflexState& reflexState) {
	if (isGame == false) // Disable reflex for editor or project settings.
		return;

	sl::Result result = this->slReflexGetState ? this->slReflexGetState(reflexState) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

sl::FrameToken* StreamlineContext::get_frame_token() {
	sl::FrameToken* token = nullptr;
	sl::Result result = this->slGetNewFrameToken ? this->slGetNewFrameToken(token, nullptr) : sl::Result::eOk;
	ERR_FAIL_COND_V_MSG(result != sl::Result::eOk, nullptr, StreamlineContext::result_to_string(result));
	last_token = token;
	return token;
}


const char* StreamlineContext::result_to_string(sl::Result result) {
    switch (result) {
        case sl::Result::eOk: return "sl::eOk";
        case sl::Result::eErrorIO: return "sl::eErrorIO";
        case sl::Result::eErrorDriverOutOfDate: return "sl::eErrorDriverOutOfDate";
        case sl::Result::eErrorOSOutOfDate: return "sl::eErrorOSOutOfDate";
        case sl::Result::eErrorOSDisabledHWS: return "sl::eErrorOSDisabledHWS";
        case sl::Result::eErrorDeviceNotCreated: return "sl::eErrorDeviceNotCreated";
        case sl::Result::eErrorNoSupportedAdapterFound: return "sl::eErrorNoSupportedAdapterFound";
        case sl::Result::eErrorAdapterNotSupported: return "sl::eErrorAdapterNotSupported";
        case sl::Result::eErrorNoPlugins: return "sl::eErrorNoPlugins";
        case sl::Result::eErrorVulkanAPI: return "sl::eErrorVulkanAPI";
        case sl::Result::eErrorDXGIAPI: return "sl::eErrorDXGIAPI";
        case sl::Result::eErrorD3DAPI: return "sl::eErrorD3DAPI";
        case sl::Result::eErrorNRDAPI: return "sl::eErrorNRDAPI";
        case sl::Result::eErrorNVAPI: return "sl::eErrorNVAPI";
        case sl::Result::eErrorReflexAPI: return "sl::eErrorReflexAPI";
        case sl::Result::eErrorNGXFailed: return "sl::eErrorNGXFailed";
        case sl::Result::eErrorJSONParsing: return "sl::eErrorJSONParsing";
        case sl::Result::eErrorMissingProxy: return "sl::eErrorMissingProxy";
        case sl::Result::eErrorMissingResourceState: return "sl::eErrorMissingResourceState";
        case sl::Result::eErrorInvalidIntegration: return "sl::eErrorInvalidIntegration";
        case sl::Result::eErrorMissingInputParameter: return "sl::eErrorMissingInputParameter";
        case sl::Result::eErrorNotInitialized: return "sl::eErrorNotInitialized";
        case sl::Result::eErrorComputeFailed: return "sl::eErrorComputeFailed";
        case sl::Result::eErrorInitNotCalled: return "sl::eErrorInitNotCalled";
        case sl::Result::eErrorExceptionHandler: return "sl::eErrorExceptionHandler";
        case sl::Result::eErrorInvalidParameter: return "sl::eErrorInvalidParameter";
        case sl::Result::eErrorMissingConstants: return "sl::eErrorMissingConstants";
        case sl::Result::eErrorDuplicatedConstants: return "sl::eErrorDuplicatedConstants";
        case sl::Result::eErrorMissingOrInvalidAPI: return "sl::eErrorMissingOrInvalidAPI";
        case sl::Result::eErrorCommonConstantsMissing: return "sl::eErrorCommonConstantsMissing";
        case sl::Result::eErrorUnsupportedInterface: return "sl::eErrorUnsupportedInterface";
        case sl::Result::eErrorFeatureMissing: return "sl::eErrorFeatureMissing";
        case sl::Result::eErrorFeatureNotSupported: return "sl::eErrorFeatureNotSupported";
        case sl::Result::eErrorFeatureMissingHooks: return "sl::eErrorFeatureMissingHooks";
        case sl::Result::eErrorFeatureFailedToLoad: return "sl::eErrorFeatureFailedToLoad";
        case sl::Result::eErrorFeatureWrongPriority: return "sl::eErrorFeatureWrongPriority";
        case sl::Result::eErrorFeatureMissingDependency: return "sl::eErrorFeatureMissingDependency";
        case sl::Result::eErrorFeatureManagerInvalidState: return "sl::eErrorFeatureManagerInvalidState";
        case sl::Result::eErrorInvalidState: return "sl::eErrorInvalidState";
        case sl::Result::eWarnOutOfVRAM: return "sl::eWarnOutOfVRAM";
        default: return "sl::eUnknown";
    }
}
#endif // STREAMLINE_IMPLEMENTATION

#endif // USE_STREAMLINE

#ifdef STREAMLINE_IMPLEMENTATION

void VulkanContext::streamline_initialize() {
#if USE_STREAMLINE
	StreamlineContext::get().isGame = true;
	if (Engine::get_singleton()->is_editor_hint() == true || Engine::get_singleton()->is_project_manager_hint() == true)
		StreamlineContext::get().isGame = false;

	if(StreamlineContext::get().slInit != nullptr)
		return; // already initialized.

	StreamlineContext::get().load_functions();
	if (StreamlineContext::get().slInit == nullptr) {
		print_line("Streamline: Could not find slInit. Did the module load correctly?");
		return;
	}

	sl::Preferences pref;

    Vector<sl::Feature> featuresToLoad;
	featuresToLoad.push_back(sl::kFeatureDLSS);
	if(StreamlineContext::get().isGame)
	{
		featuresToLoad.push_back(sl::kFeatureDLSS_G);
		featuresToLoad.push_back(sl::kFeatureReflex);
	}
	pref.featuresToLoad = featuresToLoad.ptr();
	pref.numFeaturesToLoad = featuresToLoad.size();
	pref.renderAPI = sl::RenderAPI::eVulkan;
	
	if(bool(GLOBAL_GET("rendering/nvidia/streamline_log")) == true)
	{
		pref.logLevel = sl::LogLevel::eVerbose;
		pref.showConsole = true;
	}
	else
	{
		pref.logLevel = sl::LogLevel::eOff;
		pref.showConsole = false;
	}
	sl::Result result = StreamlineContext::get().slInit(pref, sl::kSDKVersion);
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));

	StreamlineContext::get().load_functions_post_init();
#endif
}

void VulkanContext::streamline_destroy() {
#if USE_STREAMLINE
	if (StreamlineContext::get().slInit == nullptr || StreamlineContext::get().slShutdown == nullptr)
		return; // not initialized. skip.

	sl::Result result = StreamlineContext::get().slShutdown();
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
#endif
}

void VulkanContext::streamline_enumerate_capabilities() {
#if USE_STREAMLINE
	if (StreamlineContext::get().slInit == nullptr)
		return; // not initialized. skip.

	streamline_capabilities = StreamlineContext::get().enumerate_support(gpu);
#endif
}

void VulkanContext::streamline_init_post_device() {
#if USE_STREAMLINE
	if(streamline_capabilities.reflexAvailable)
	{
		sl::ReflexOptions reflex_options = {};
		reflex_options.frameLimitUs = 0;
		reflex_options.virtualKey = 0x7C; // VK_F13;
		reflex_options.mode = sl::ReflexMode::eOff;
		reflex_options.useMarkersToOptimize = true;
		reflex_options.idThread = 0;
		StreamlineContext::get().reflex_set_options(reflex_options);
	}

	set_nvidia_parameter(RD::NV_PARAM_REFLEX_MODE, (double)GLOBAL_GET("rendering/nvidia/reflex_mode"));
	set_nvidia_parameter(RD::NV_PARAM_REFLEX_FRAME_LIMIT_US, (double)GLOBAL_GET("rendering/nvidia/reflex_frame_limit_us"));
#endif
}

void VulkanContext::set_nvidia_parameter(RenderingDevice::NvidiaParameter parameterType, const Variant &value) {
	switch (parameterType) {
#if USE_STREAMLINE
		case RD::NV_PARAM_REFLEX_MODE: {
			double val = (double)value;
			sl::ReflexMode newMode;
			if (val > 1.0)
				newMode = sl::ReflexMode::eLowLatencyWithBoost;
			else if (val > 0.0)
				newMode = sl::ReflexMode::eLowLatency;
			else
				newMode = sl::ReflexMode::eOff;

			if (StreamlineContext::get().reflex_options.mode != newMode)
				StreamlineContext::get().reflex_options_dirty = true;
			StreamlineContext::get().reflex_options.mode = newMode;
			break;
		}
		case RD::NV_PARAM_REFLEX_FRAME_LIMIT_US: {
			double val = (double)value;
			bool updated = StreamlineContext::get().reflex_options.frameLimitUs != (unsigned int)val;
			StreamlineContext::get().reflex_options.frameLimitUs = (unsigned int)val;
			if (updated)
				StreamlineContext::get().reflex_options_dirty = true;
			break;
		}
#endif
		default:
			break;
	}
}

void VulkanContext::streamline_emit(RenderingDevice::MarkerType marker) {
#if USE_STREAMLINE
	if (StreamlineContext::get().isGame == false || streamline_capabilities.reflexAvailable == false)
	{
		// Make sure we still get frame tokens, needed for DLSS.
		if(marker == RenderingDevice::BeforeMessageLoop)
			streamline_framemarker = StreamlineContext::get().get_frame_token();

		return;
	}

	static unsigned long long lastPingFrameDetected = 0; // TODO: Do this differently. Multi window?
	sl::ReflexMarker sl_marker;
	switch(marker)
	{
		case RenderingDevice::BeforeMessageLoop:
			if (StreamlineContext::get().reflex_options_dirty)
				StreamlineContext::get().reflex_set_options(StreamlineContext::get().reflex_options);
			streamline_framemarker = StreamlineContext::get().get_frame_token();
			if (StreamlineContext::get().reflex_options.mode != sl::ReflexMode::eOff || StreamlineContext::get().reflex_options.frameLimitUs > 0)
				StreamlineContext::get().reflex_sleep((sl::FrameToken *)streamline_framemarker);
			sl_marker = sl::ReflexMarker::eInputSample;
			break;
		case RenderingDevice::BeginRender:
			sl_marker = sl::ReflexMarker::eRenderSubmitStart; break;
		case RenderingDevice::EndRender:
			sl_marker = sl::ReflexMarker::eRenderSubmitEnd; break;
		case RenderingDevice::BeginSimulation:
			// Handle ping events before simulation starts
			if (Input::get_singleton()->get_last_ping_frame() != lastPingFrameDetected) {
				lastPingFrameDetected = Input::get_singleton()->get_last_ping_frame();
				streamline_emit(RenderingDevice::PcPing);
			}

			sl_marker = sl::ReflexMarker::eSimulationStart;
			break;
		case RenderingDevice::EndSimulation:
			sl_marker = sl::ReflexMarker::eSimulationEnd; break;
		case RenderingDevice::BeginPresent:
			sl_marker = sl::ReflexMarker::ePresentStart; break;
		case RenderingDevice::EndPresent:
			sl_marker = sl::ReflexMarker::ePresentEnd; break;
		case RenderingDevice::PcPing:
			sl_marker = sl::ReflexMarker::ePCLatencyPing; break;
	};
	if(streamline_framemarker)
		StreamlineContext::get().reflex_marker((sl::FrameToken *)streamline_framemarker, sl_marker);
#endif
}

#endif // STREAMLINE_IMPLEMENTATION

#ifdef STREAMLINE_RD_IMPLEMENTATION

unsigned long long StreamlineContext::convert_rdformat_to_vkformat(RenderingDevice::DataFormat format)
{
	return RenderingDeviceVulkan::vulkan_formats[(int)format];
}

#endif // STREAMLINE_RD_IMPLEMENTATION
