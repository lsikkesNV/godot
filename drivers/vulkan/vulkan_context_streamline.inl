#ifdef USE_STREAMLINE
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>

class StreamlineContext
{
public:
	// Interposer
	PFun_slInit* slInit = nullptr;
	PFun_slIsFeatureSupported* slIsFeatureSupported = nullptr;
	PFun_slGetFeatureFunction* slGetFeatureFunction = nullptr;
	PFun_slGetNewFrameToken* slGetNewFrameToken = nullptr;

	// Reflex
	sl::ReflexOptions reflex_options;
	PFun_slReflexGetState* slReflexGetState = nullptr;
	PFun_slReflexSetOptions* slReflexSetOptions = nullptr;
	PFun_slReflexSetMarker* slReflexSetMarker = nullptr;
	PFun_slReflexSleep* slReflexSleep = nullptr;

	void load_functions();
	void load_functions_post_init();
	VulkanContext::StreamlineCapabilities enumerate_support(VkPhysicalDevice device);
	static const char* result_to_string(sl::Result result);
	void reflex_set_options(const sl::ReflexOptions& opts);
	void reflex_marker(sl::FrameToken* frameToken, sl::ReflexMarker marker);
	void reflex_sleep(sl::FrameToken* frameToken);
	void reflex_get_state(sl::ReflexState& reflexState);
	sl::FrameToken* get_frame_token();
} gStreamline;


void StreamlineContext::load_functions() {
#ifdef _WIN32
	HMODULE streamline = LoadLibraryA("sl.interposer.dll");
	if(streamline == nullptr)
		return;
	this->slInit = (PFun_slInit*)GetProcAddress(streamline, "slInit");
	this->slIsFeatureSupported = (PFun_slIsFeatureSupported*)GetProcAddress(streamline, "slIsFeatureSupported");
	this->slGetFeatureFunction = (PFun_slGetFeatureFunction*)GetProcAddress(streamline, "slGetFeatureFunction");
	this->slGetNewFrameToken = (PFun_slGetNewFrameToken*)GetProcAddress(streamline, "slGetNewFrameToken");
#endif
}

void StreamlineContext::load_functions_post_init() {
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetOptions", (void*&)this->slReflexSetOptions);
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetMarker", (void*&)this->slReflexSetMarker);
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexSleep", (void*&)this->slReflexSleep);
	slGetFeatureFunction(sl::kFeatureReflex, "slReflexGetState", (void*&)this->slReflexGetState);

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
	reflex_options = opts;
	sl::Result result = this->slReflexSetOptions ? this->slReflexSetOptions(opts) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

void StreamlineContext::reflex_marker(sl::FrameToken* frameToken, sl::ReflexMarker marker) {
	sl::Result result = this->slReflexSetMarker ? this->slReflexSetMarker(marker, *frameToken) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

void StreamlineContext::reflex_sleep(sl::FrameToken* frameToken) {
	if(frameToken == nullptr)
		return;
	sl::Result result = this->slReflexSleep ? this->slReflexSleep(*frameToken) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

void StreamlineContext::reflex_get_state(sl::ReflexState& reflexState) {
	sl::Result result = this->slReflexGetState ? this->slReflexGetState(reflexState) : sl::Result::eOk;
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));
}

sl::FrameToken* StreamlineContext::get_frame_token() {
	sl::FrameToken* token = nullptr;
	sl::Result result = this->slGetNewFrameToken ? this->slGetNewFrameToken(token, nullptr) : sl::Result::eOk;
	ERR_FAIL_COND_V_MSG(result != sl::Result::eOk, nullptr, StreamlineContext::result_to_string(result));
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

#endif

void VulkanContext::streamline_initialize() {
#ifdef USE_STREAMLINE
	if(gStreamline.slInit != nullptr)
		return; // already initialized.

	gStreamline.load_functions();
	if(gStreamline.slInit == nullptr) {
		print_line("Streamline: Could not find slInit. Did the module load correctly?");
		return;
	}

	sl::Preferences pref;
    sl::Feature featuresToLoad[] = { 
        sl::kFeatureDLSS,
        sl::kFeatureDLSS_G,
        sl::kFeatureReflex
    };
	pref.featuresToLoad = featuresToLoad;
	pref.numFeaturesToLoad = sizeof(featuresToLoad) / sizeof(featuresToLoad[0]);
	pref.renderAPI = sl::RenderAPI::eVulkan;
	pref.showConsole = true;
	pref.logLevel = sl::LogLevel::eVerbose;
	sl::Result result = gStreamline.slInit(pref, sl::kSDKVersion);
	ERR_FAIL_COND_MSG(result != sl::Result::eOk, StreamlineContext::result_to_string(result));

	gStreamline.load_functions_post_init();

#endif
}

void VulkanContext::streamline_enumerate_capabilities() {
#ifdef USE_STREAMLINE
	if(gStreamline.slInit == nullptr)
		return; // not initialized. skip.

	streamline_capabilities = gStreamline.enumerate_support(gpu);
#endif
}

void VulkanContext::streamline_init_post_device() {
#ifdef USE_STREAMLINE
	if(streamline_capabilities.reflexAvailable)
	{
		sl::ReflexOptions reflex_options = {};
		reflex_options.frameLimitUs = 0;
		reflex_options.virtualKey = 0x7C; // VK_F13;
		reflex_options.mode = sl::ReflexMode::eLowLatency;
		reflex_options.useMarkersToOptimize = true;
		reflex_options.idThread = 0;
		gStreamline.reflex_set_options(reflex_options);
	}
#endif
}

void VulkanContext::streamline_emit(RenderingDevice::MarkerType marker) {
#ifdef USE_STREAMLINE
	if(streamline_capabilities.reflexAvailable == false)
		return;

	static unsigned long long lastPingFrameDetected = 0; // TODO: Do this differently? Multi window?
	sl::ReflexMarker sl_marker;
	switch(marker)
	{
		case RenderingDevice::BeforeMessageLoop:
			streamline_framemarker = gStreamline.get_frame_token();
			gStreamline.reflex_sleep((sl::FrameToken*)streamline_framemarker);
			sl_marker = sl::ReflexMarker::eInputSample; 
			break;
		case RenderingDevice::BeginRender:
			sl_marker = sl::ReflexMarker::eRenderSubmitStart; break;
		case RenderingDevice::EndRender:
			sl_marker = sl::ReflexMarker::eRenderSubmitEnd; break;
		case RenderingDevice::BeginSimulation:
			// Handle ping events before simulation starts
			if(Input::get_singleton()->get_last_ping_frame() != lastPingFrameDetected) {
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
		gStreamline.reflex_marker((sl::FrameToken*)streamline_framemarker, sl_marker);
#endif
}
