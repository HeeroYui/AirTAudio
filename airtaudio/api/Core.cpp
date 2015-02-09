/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */


// *************************************************** //
//
// OS/API-specific methods.
//
// *************************************************** //

#if defined(__MACOSX_CORE__) || defined(__IOS_CORE__)

#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>

airtaudio::Api* airtaudio::api::Core::Create() {
	return new airtaudio::api::Core();
}

#undef __class__
#define __class__ "api::Core"

// The OS X CoreAudio API is designed to use a separate callback
// procedure for each of its audio devices.	A single RtAudio duplex
// stream using two different devices is supported here, though it
// cannot be guaranteed to always behave correctly because we cannot
// synchronize these two callbacks.
//
// A property listener is installed for over/underrun information.
// However, no functionality is currently provided to allow property
// listeners to trigger user handlers because it is unclear what could
// be done if a critical stream parameter (buffer size, sample rate,
// device disconnect) notification arrived.	The listeners entail
// quite a bit of extra code and most likely, a user program wouldn't
// be prepared for the result anyway.	However, we do provide a flag
// to the client callback function to inform of an over/underrun.

// A structure to hold various information related to the CoreAudio API
// implementation.
struct CoreHandle {
	AudioDeviceID id[2]; // device ids
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5)
	AudioDeviceIOProcID procId[2];
#endif
	uint32_t iStream[2]; // device stream index (or first if using multiple)
	uint32_t nStreams[2]; // number of streams to use
	bool xrun[2];
	char *deviceBuffer;
	std::condition_variable condition;
	int32_t drainCounter; // Tracks callback counts when draining
	bool internalDrain; // Indicates if stop is initiated from callback or not.
	CoreHandle() :
	  deviceBuffer(0),
	  drainCounter(0),
	  internalDrain(false) {
		nStreams[0] = 1;
		nStreams[1] = 1;
		id[0] = 0;
		id[1] = 0;
		xrun[0] = false;
		xrun[1] = false;
	}
};

airtaudio::api::Core::Core() {
#if defined(AVAILABLE_MAC_OS_X_VERSION_10_6_AND_LATER)
	// This is a largely undocumented but absolutely necessary
	// requirement starting with OS-X 10.6.	If not called, queries and
	// updates to various audio device properties are not handled
	// correctly.
	CFRunLoopRef theRunLoop = nullptr;
	AudioObjectPropertyAddress property = {
		kAudioHardwarePropertyRunLoop,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	OSStatus result = AudioObjectSetPropertyData(kAudioObjectSystemObject,
	                                             &property,
	                                             0,
	                                             nullptr,
	                                             sizeof(CFRunLoopRef),
	                                             &theRunLoop);
	if (result != noErr) {
		ATA_ERROR("error setting run loop property!");
	}
#endif
}

airtaudio::api::Core::~Core() {
	// The subclass destructor gets called before the base class
	// destructor, so close an existing stream before deallocating
	// apiDeviceId memory.
	if (m_state != airtaudio::state_closed) {
		closeStream();
	}
}

uint32_t airtaudio::api::Core::getDeviceCount() {
	// Find out how many audio devices there are, if any.
	uint32_t dataSize;
	AudioObjectPropertyAddress propertyAddress = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	OSStatus result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &dataSize);
	if (result != noErr) {
		ATA_ERROR("OS-X error getting device info!");
		return 0;
	}
	return dataSize / sizeof(AudioDeviceID);
}

uint32_t airtaudio::api::Core::getDefaultInputDevice() {
	uint32_t nDevices = getDeviceCount();
	if (nDevices <= 1) {
		return 0;
	}
	AudioDeviceID id;
	uint32_t dataSize = sizeof(AudioDeviceID);
	AudioObjectPropertyAddress property = {
		kAudioHardwarePropertyDefaultInputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
	                                             &property,
	                                             0,
	                                             nullptr,
	                                             &dataSize,
	                                             &id);
	if (result != noErr) {
		ATA_ERROR("OS-X system error getting device.");
		return 0;
	}
	dataSize *= nDevices;
	AudioDeviceID deviceList[ nDevices ];
	property.mSelector = kAudioHardwarePropertyDevices;
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
	                                    &property,
	                                    0,
	                                    nullptr,
	                                    &dataSize,
	                                    (void*)&deviceList);
	if (result != noErr) {
		ATA_ERROR("OS-X system error getting device IDs.");
		return 0;
	}
	for (uint32_t iii=0; iii<nDevices; iii++) {
		if (id == deviceList[iii]) {
			return iii;
		}
	}
	ATA_ERROR("No default device found!");
	return 0;
}

uint32_t airtaudio::api::Core::getDefaultOutputDevice() {
	uint32_t nDevices = getDeviceCount();
	if (nDevices <= 1) {
		return 0;
	}
	AudioDeviceID id;
	uint32_t dataSize = sizeof(AudioDeviceID);
	AudioObjectPropertyAddress property = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
	                                             &property,
	                                             0,
	                                             nullptr,
	                                             &dataSize,
	                                             &id);
	if (result != noErr) {
		ATA_ERROR("OS-X system error getting device.");
		return 0;
	}
	dataSize = sizeof(AudioDeviceID) * nDevices;
	AudioDeviceID deviceList[ nDevices ];
	property.mSelector = kAudioHardwarePropertyDevices;
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
	                                    &property,
	                                    0,
	                                    nullptr,
	                                    &dataSize,
	                                    (void*)&deviceList);
	if (result != noErr) {
		ATA_ERROR("OS-X system error getting device IDs.");
		return 0;
	}
	for (uint32_t iii=0; iii<nDevices; iii++) {
		if (id == deviceList[iii]) {
			return iii;
		}
	}
	ATA_ERROR("No default device found!");
	return 0;
}

airtaudio::DeviceInfo airtaudio::api::Core::getDeviceInfo(uint32_t _device) {
	airtaudio::DeviceInfo info;
	info.probed = false;
	// Get device ID
	uint32_t nDevices = getDeviceCount();
	if (nDevices == 0) {
		ATA_ERROR("no devices found!");
		return info;
	}
	if (_device >= nDevices) {
		ATA_ERROR("device ID is invalid!");
		return info;
	}
	AudioDeviceID deviceList[ nDevices ];
	uint32_t dataSize = sizeof(AudioDeviceID) * nDevices;
	AudioObjectPropertyAddress property = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
	                                             &property,
	                                             0,
	                                             nullptr,
	                                             &dataSize,
	                                             (void*)&deviceList);
	if (result != noErr) {
		ATA_ERROR("OS-X system error getting device IDs.");
		return info;
	}
	AudioDeviceID id = deviceList[ _device ];
	// Get the device name.
	info.name.erase();
	CFStringRef cfname;
	dataSize = sizeof(CFStringRef);
	property.mSelector = kAudioObjectPropertyManufacturer;
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &cfname);
	if (result != noErr) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting device manufacturer.");
		return info;
	}
	//const char *mname = CFStringGetCStringPtr(cfname, CFStringGetSystemEncoding());
	int32_t length = CFStringGetLength(cfname);
	char *mname = (char *)malloc(length * 3 + 1);
	CFStringGetCString(cfname, mname, length * 3 + 1, CFStringGetSystemEncoding());
	info.name.append((const char *)mname, strlen(mname));
	info.name.append(": ");
	CFRelease(cfname);
	free(mname);
	property.mSelector = kAudioObjectPropertyName;
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &cfname);
	if (result != noErr) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting device name.");
		return info;
	}
	//const char *name = CFStringGetCStringPtr(cfname, CFStringGetSystemEncoding());
	length = CFStringGetLength(cfname);
	char *name = (char *)malloc(length * 3 + 1);
	CFStringGetCString(cfname, name, length * 3 + 1, CFStringGetSystemEncoding());
	info.name.append((const char *)name, strlen(name));
	CFRelease(cfname);
	free(name);
	// Get the output stream "configuration".
	AudioBufferList	*bufferList = nil;
	property.mSelector = kAudioDevicePropertyStreamConfiguration;
	property.mScope = kAudioDevicePropertyScopeOutput;
	//	property.mElement = kAudioObjectPropertyElementWildcard;
	dataSize = 0;
	result = AudioObjectGetPropertyDataSize(id, &property, 0, nullptr, &dataSize);
	if (result != noErr || dataSize == 0) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting output stream configuration info for device (" << _device << ").");
		return info;
	}
	// Allocate the AudioBufferList.
	bufferList = (AudioBufferList *) malloc(dataSize);
	if (bufferList == nullptr) {
		ATA_ERROR("memory error allocating output AudioBufferList.");
		return info;
	}
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, bufferList);
	if (    result != noErr
	     || dataSize == 0) {
		free(bufferList);
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting output stream configuration for device (" << _device << ").");
		return info;
	}
	// Get output channel information.
	uint32_t i, nStreams = bufferList->mNumberBuffers;
	for (i=0; i<nStreams; i++) {
		info.outputChannels += bufferList->mBuffers[i].mNumberChannels;
	}
	free(bufferList);
	// Get the input stream "configuration".
	property.mScope = kAudioDevicePropertyScopeInput;
	result = AudioObjectGetPropertyDataSize(id, &property, 0, nullptr, &dataSize);
	if (    result != noErr
	     || dataSize == 0) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting input stream configuration info for device (" << _device << ").");
		return info;
	}
	// Allocate the AudioBufferList.
	bufferList = (AudioBufferList *) malloc(dataSize);
	if (bufferList == nullptr) {
		ATA_ERROR("memory error allocating input AudioBufferList.");
		return info;
	}
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, bufferList);
	if (result != noErr || dataSize == 0) {
		free(bufferList);
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting input stream configuration for device (" << _device << ").");
		return info;
	}
	// Get input channel information.
	nStreams = bufferList->mNumberBuffers;
	for (i=0; i<nStreams; i++) {
		info.inputChannels += bufferList->mBuffers[i].mNumberChannels;
	}
	free(bufferList);
	// If device opens for both playback and capture, we determine the channels.
	if (    info.outputChannels > 0
	     && info.inputChannels > 0) {
		info.duplexChannels = (info.outputChannels > info.inputChannels) ? info.inputChannels : info.outputChannels;
	}
	// Probe the device sample rates.
	bool isInput = false;
	if (info.outputChannels == 0) {
		isInput = true;
	}
	// Determine the supported sample rates.
	property.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
	if (isInput == false) property.mScope = kAudioDevicePropertyScopeOutput;
	result = AudioObjectGetPropertyDataSize(id, &property, 0, nullptr, &dataSize);
	if (    result != kAudioHardwareNoError
	     || dataSize == 0) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting sample rate info.");
		return info;
	}
	uint32_t nRanges = dataSize / sizeof(AudioValueRange);
	AudioValueRange rangeList[ nRanges ];
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &rangeList);
	if (result != kAudioHardwareNoError) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting sample rates.");
		return info;
	}
	double minimumRate = 100000000.0, maximumRate = 0.0;
	for (uint32_t i=0; i<nRanges; i++) {
		if (rangeList[i].mMinimum < minimumRate) {
			minimumRate = rangeList[i].mMinimum;
		}
		if (rangeList[i].mMaximum > maximumRate) {
			maximumRate = rangeList[i].mMaximum;
		}
	}
	info.sampleRates.clear();
	for (uint32_t k=0; k<MAX_SAMPLE_RATES; k++) {
		if (SAMPLE_RATES[k] >= (uint32_t) minimumRate && SAMPLE_RATES[k] <= (uint32_t) maximumRate) {
			info.sampleRates.push_back(SAMPLE_RATES[k]);
		}
	}
	if (info.sampleRates.size() == 0) {
		ATA_ERROR("No supported sample rates found for device (" << _device << ").");
		return info;
	}
	// CoreAudio always uses 32-bit floating point data for PCM streams.
	// Thus, any other "physical" formats supported by the device are of
	// no interest to the client.
	info.nativeFormats.push_back(audio::format_float);
	if (info.outputChannels > 0) {
		if (getDefaultOutputDevice() == _device) {
			info.isDefaultOutput = true;
		}
	}
	if (info.inputChannels > 0) {
		if (getDefaultInputDevice() == _device) {
			info.isDefaultInput = true;
		}
	}
	info.probed = true;
	return info;
}

OSStatus airtaudio::api::Core::callbackEvent(AudioDeviceID _inDevice,
                                             const AudioTimeStamp* _inNow,
                                             const AudioBufferList* _inInputData,
                                             const AudioTimeStamp* _inInputTime,
                                             AudioBufferList* _outOutputData,
                                             const AudioTimeStamp* _inOutputTime,
                                             void* _userData) {
	airtaudio::api::Core* myClass = reinterpret_cast<airtaudio::api::Core*>(_userData);
	if (myClass->callbackEvent(_inDevice, _inInputData, _outOutputData) == false) {
		return kAudioHardwareUnspecifiedError;
	} else {
		return kAudioHardwareNoError;
	}
}

static OSStatus xrunListener(AudioObjectID _inDevice,
                             uint32_t _nAddresses,
                             const AudioObjectPropertyAddress _properties[],
                             void* _handlePointer) {
	CoreHandle* handle = (CoreHandle*)_handlePointer;
	for (uint32_t i=0; i<_nAddresses; i++) {
		if (_properties[i].mSelector == kAudioDeviceProcessorOverload) {
			if (_properties[i].mScope == kAudioDevicePropertyScopeInput) {
				handle->xrun[1] = true;
			} else {
				handle->xrun[0] = true;
			}
		}
	}
	return kAudioHardwareNoError;
}

static OSStatus rateListener(AudioObjectID _inDevice,
                             uint32_t _nAddresses,
                             const AudioObjectPropertyAddress _properties[],
                             void* _ratePointer) {
	double *rate = (double*)_ratePointer;
	uint32_t dataSize = sizeof(double);
	AudioObjectPropertyAddress property = {
		kAudioDevicePropertyNominalSampleRate,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	AudioObjectGetPropertyData(_inDevice, &property, 0, nullptr, &dataSize, rate);
	return kAudioHardwareNoError;
}

bool airtaudio::api::Core::probeDeviceOpen(uint32_t _device,
                                           airtaudio::mode _mode,
                                           uint32_t _channels,
                                           uint32_t _firstChannel,
                                           uint32_t _sampleRate,
                                           audio::format _format,
                                           uint32_t *_bufferSize,
                                           airtaudio::StreamOptions *_options) {
	// Get device ID
	uint32_t nDevices = getDeviceCount();
	if (nDevices == 0) {
		// This should not happen because a check is made before this function is called.
		ATA_ERROR("no devices found!");
		return false;
	}
	if (_device >= nDevices) {
		// This should not happen because a check is made before this function is called.
		ATA_ERROR("device ID is invalid!");
		return false;
	}
	AudioDeviceID deviceList[ nDevices ];
	uint32_t dataSize = sizeof(AudioDeviceID) * nDevices;
	AudioObjectPropertyAddress property = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
	                                             &property,
	                                             0,
	                                             nullptr,
	                                             &dataSize,
	                                             (void *) &deviceList);
	if (result != noErr) {
		ATA_ERROR("OS-X system error getting device IDs.");
		return false;
	}
	AudioDeviceID id = deviceList[ _device ];
	// Setup for stream mode.
	bool isInput = false;
	if (_mode == airtaudio::mode_input) {
		isInput = true;
		property.mScope = kAudioDevicePropertyScopeInput;
	} else {
		property.mScope = kAudioDevicePropertyScopeOutput;
	}
	// Get the stream "configuration".
	AudioBufferList	*bufferList = nil;
	dataSize = 0;
	property.mSelector = kAudioDevicePropertyStreamConfiguration;
	result = AudioObjectGetPropertyDataSize(id, &property, 0, nullptr, &dataSize);
	if (    result != noErr
	     || dataSize == 0) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting stream configuration info for device (" << _device << ").");
		return false;
	}
	// Allocate the AudioBufferList.
	bufferList = (AudioBufferList *) malloc(dataSize);
	if (bufferList == nullptr) {
		ATA_ERROR("memory error allocating AudioBufferList.");
		return false;
	}
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, bufferList);
	if (    result != noErr
	     || dataSize == 0) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting stream configuration for device (" << _device << ").");
		return false;
	}
	// Search for one or more streams that contain the desired number of
	// channels. CoreAudio devices can have an arbitrary number of
	// streams and each stream can have an arbitrary number of channels.
	// For each stream, a single buffer of interleaved samples is
	// provided.	RtAudio prefers the use of one stream of interleaved
	// data or multiple consecutive single-channel streams.	However, we
	// now support multiple consecutive multi-channel streams of
	// interleaved data as well.
	uint32_t iStream, offsetCounter = _firstChannel;
	uint32_t nStreams = bufferList->mNumberBuffers;
	bool monoMode = false;
	bool foundStream = false;
	// First check that the device supports the requested number of
	// channels.
	uint32_t deviceChannels = 0;
	for (iStream=0; iStream<nStreams; iStream++) {
		deviceChannels += bufferList->mBuffers[iStream].mNumberChannels;
	}
	if (deviceChannels < (_channels + _firstChannel)) {
		free(bufferList);
		ATA_ERROR("the device (" << _device << ") does not support the requested channel count.");
		return false;
	}
	// Look for a single stream meeting our needs.
	uint32_t firstStream, streamCount = 1, streamChannels = 0, channelOffset = 0;
	for (iStream=0; iStream<nStreams; iStream++) {
		streamChannels = bufferList->mBuffers[iStream].mNumberChannels;
		if (streamChannels >= _channels + offsetCounter) {
			firstStream = iStream;
			channelOffset = offsetCounter;
			foundStream = true;
			break;
		}
		if (streamChannels > offsetCounter) {
			break;
		}
		offsetCounter -= streamChannels;
	}
	// If we didn't find a single stream above, then we should be able
	// to meet the channel specification with multiple streams.
	if (foundStream == false) {
		monoMode = true;
		offsetCounter = _firstChannel;
		for (iStream=0; iStream<nStreams; iStream++) {
			streamChannels = bufferList->mBuffers[iStream].mNumberChannels;
			if (streamChannels > offsetCounter) {
				break;
			}
			offsetCounter -= streamChannels;
		}
		firstStream = iStream;
		channelOffset = offsetCounter;
		int32_t channelCounter = _channels + offsetCounter - streamChannels;
		if (streamChannels > 1) {
			monoMode = false;
		}
		while (channelCounter > 0) {
			streamChannels = bufferList->mBuffers[++iStream].mNumberChannels;
			if (streamChannels > 1) {
				monoMode = false;
			}
			channelCounter -= streamChannels;
			streamCount++;
		}
	}
	free(bufferList);
	// Determine the buffer size.
	AudioValueRange	bufferRange;
	dataSize = sizeof(AudioValueRange);
	property.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &bufferRange);
	if (result != noErr) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting buffer size range for device (" << _device << ").");
		return false;
	}
	if (bufferRange.mMinimum > *_bufferSize) {
		*_bufferSize = (uint64_t) bufferRange.mMinimum;
	} else if (bufferRange.mMaximum < *_bufferSize) {
		*_bufferSize = (uint64_t) bufferRange.mMaximum;
	}
	if (    _options != nullptr
	     && _options->flags.m_minimizeLatency == true) {
		*_bufferSize = (uint64_t) bufferRange.mMinimum;
	}
	// Set the buffer size.	For multiple streams, I'm assuming we only
	// need to make this setting for the master channel.
	uint32_t theSize = (uint32_t) *_bufferSize;
	dataSize = sizeof(uint32_t);
	property.mSelector = kAudioDevicePropertyBufferFrameSize;
	result = AudioObjectSetPropertyData(id, &property, 0, nullptr, dataSize, &theSize);
	if (result != noErr) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") setting the buffer size for device (" << _device << ").");
		return false;
	}
	// If attempting to setup a duplex stream, the bufferSize parameter
	// MUST be the same in both directions!
	*_bufferSize = theSize;
	if (    m_mode == airtaudio::mode_output
	     && _mode == airtaudio::mode_input
	     && *_bufferSize != m_bufferSize) {
		ATA_ERROR("system error setting buffer size for duplex stream on device (" << _device << ").");
		return false;
	}
	m_bufferSize = *_bufferSize;
	m_nBuffers = 1;
	// Try to set "hog" mode ... it's not clear to me this is working.
	if (    _options != nullptr
	     && _options->flags & HOG_DEVICE) {
		pid_t hog_pid;
		dataSize = sizeof(hog_pid);
		property.mSelector = kAudioDevicePropertyHogMode;
		result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &hog_pid);
		if (result != noErr) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") getting 'hog' state!");
			return false;
		}
		if (hog_pid != getpid()) {
			hog_pid = getpid();
			result = AudioObjectSetPropertyData(id, &property, 0, nullptr, dataSize, &hog_pid);
			if (result != noErr) {
				ATA_ERROR("system error (" << getErrorCode(result) << ") setting 'hog' state!");
				return false;
			}
		}
	}
	// Check and if necessary, change the sample rate for the device.
	double nominalRate;
	dataSize = sizeof(double);
	property.mSelector = kAudioDevicePropertyNominalSampleRate;
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &nominalRate);
	if (result != noErr) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting current sample rate.");
		return false;
	}
	// Only change the sample rate if off by more than 1 Hz.
	if (fabs(nominalRate - (double)_sampleRate) > 1.0) {
		// Set a property listener for the sample rate change
		double reportedRate = 0.0;
		AudioObjectPropertyAddress tmp = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
		result = AudioObjectAddPropertyListener(id, &tmp, rateListener, (void *) &reportedRate);
		if (result != noErr) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") setting sample rate property listener for device (" << _device << ").");
			return false;
		}
		nominalRate = (double) _sampleRate;
		result = AudioObjectSetPropertyData(id, &property, 0, nullptr, dataSize, &nominalRate);
		if (result != noErr) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") setting sample rate for device (" << _device << ").");
			return false;
		}
		// Now wait until the reported nominal rate is what we just set.
		uint32_t microCounter = 0;
		while (reportedRate != nominalRate) {
			microCounter += 5000;
			if (microCounter > 5000000) {
				break;
			}
			usleep(5000);
		}
		// Remove the property listener.
		AudioObjectRemovePropertyListener(id, &tmp, rateListener, (void *) &reportedRate);
		if (microCounter > 5000000) {
			ATA_ERROR("timeout waiting for sample rate update for device (" << _device << ").");
			return false;
		}
	}
	// Now set the stream format for all streams.	Also, check the
	// physical format of the device and change that if necessary.
	AudioStreamBasicDescription	description;
	dataSize = sizeof(AudioStreamBasicDescription);
	property.mSelector = kAudioStreamPropertyVirtualFormat;
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &description);
	if (result != noErr) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting stream format for device (" << _device << ").");
		return false;
	}
	// Set the sample rate and data format id.	However, only make the
	// change if the sample rate is not within 1.0 of the desired
	// rate and the format is not linear pcm.
	bool updateFormat = false;
	if (fabs(description.mSampleRate - (double)_sampleRate) > 1.0) {
		description.mSampleRate = (double) _sampleRate;
		updateFormat = true;
	}
	if (description.mFormatID != kAudioFormatLinearPCM) {
		description.mFormatID = kAudioFormatLinearPCM;
		updateFormat = true;
	}
	if (updateFormat) {
		result = AudioObjectSetPropertyData(id, &property, 0, nullptr, dataSize, &description);
		if (result != noErr) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") setting sample rate or data format for device (" << _device << ").");
			return false;
		}
	}
	// Now check the physical format.
	property.mSelector = kAudioStreamPropertyPhysicalFormat;
	result = AudioObjectGetPropertyData(id, &property, 0, nullptr,	&dataSize, &description);
	if (result != noErr) {
		ATA_ERROR("system error (" << getErrorCode(result) << ") getting stream physical format for device (" << _device << ").");
		return false;
	}
	//std::cout << "Current physical stream format:" << std::endl;
	//std::cout << "	 mBitsPerChan = " << description.mBitsPerChannel << std::endl;
	//std::cout << "	 aligned high = " << (description.mFormatFlags & kAudioFormatFlagIsAlignedHigh) << ", isPacked = " << (description.mFormatFlags & kAudioFormatFlagIsPacked) << std::endl;
	//std::cout << "	 bytesPerFrame = " << description.mBytesPerFrame << std::endl;
	//std::cout << "	 sample rate = " << description.mSampleRate << std::endl;
	if (    description.mFormatID != kAudioFormatLinearPCM
	     || description.mBitsPerChannel < 16) {
		description.mFormatID = kAudioFormatLinearPCM;
		//description.mSampleRate = (double) sampleRate;
		AudioStreamBasicDescription	testDescription = description;
		uint32_t formatFlags;
		// We'll try higher bit rates first and then work our way down.
		std::vector< std::pair<uint32_t, uint32_t>	> physicalFormats;
		formatFlags = (description.mFormatFlags | kLinearPCMFormatFlagIsFloat) & ~kLinearPCMFormatFlagIsSignedInteger;
		physicalFormats.push_back(std::pair<float, uint32_t>(32, formatFlags));
		formatFlags = (description.mFormatFlags | kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked) & ~kLinearPCMFormatFlagIsFloat;
		physicalFormats.push_back(std::pair<float, uint32_t>(32, formatFlags));
		physicalFormats.push_back(std::pair<float, uint32_t>(24, formatFlags));	 // 24-bit packed
		formatFlags &= ~(kAudioFormatFlagIsPacked | kAudioFormatFlagIsAlignedHigh);
		physicalFormats.push_back(std::pair<float, uint32_t>(24.2, formatFlags)); // 24-bit in 4 bytes, aligned low
		formatFlags |= kAudioFormatFlagIsAlignedHigh;
		physicalFormats.push_back(std::pair<float, uint32_t>(24.4, formatFlags)); // 24-bit in 4 bytes, aligned high
		formatFlags = (description.mFormatFlags | kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked) & ~kLinearPCMFormatFlagIsFloat;
		physicalFormats.push_back(std::pair<float, uint32_t>(16, formatFlags));
		physicalFormats.push_back(std::pair<float, uint32_t>(8, formatFlags));
		bool setPhysicalFormat = false;
		for(uint32_t i=0; i<physicalFormats.size(); i++) {
			testDescription = description;
			testDescription.mBitsPerChannel = (uint32_t) physicalFormats[i].first;
			testDescription.mFormatFlags = physicalFormats[i].second;
			if (    (24 == (uint32_t)physicalFormats[i].first)
			     && ~(physicalFormats[i].second & kAudioFormatFlagIsPacked)) {
				testDescription.mBytesPerFrame =	4 * testDescription.mChannelsPerFrame;
			} else {
				testDescription.mBytesPerFrame =	testDescription.mBitsPerChannel/8 * testDescription.mChannelsPerFrame;
			}
			testDescription.mBytesPerPacket = testDescription.mBytesPerFrame * testDescription.mFramesPerPacket;
			result = AudioObjectSetPropertyData(id, &property, 0, nullptr, dataSize, &testDescription);
			if (result == noErr) {
				setPhysicalFormat = true;
				//std::cout << "Updated physical stream format:" << std::endl;
				//std::cout << "	 mBitsPerChan = " << testDescription.mBitsPerChannel << std::endl;
				//std::cout << "	 aligned high = " << (testDescription.mFormatFlags & kAudioFormatFlagIsAlignedHigh) << ", isPacked = " << (testDescription.mFormatFlags & kAudioFormatFlagIsPacked) << std::endl;
				//std::cout << "	 bytesPerFrame = " << testDescription.mBytesPerFrame << std::endl;
				//std::cout << "	 sample rate = " << testDescription.mSampleRate << std::endl;
				break;
			}
		}
		if (!setPhysicalFormat) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") setting physical data format for device (" << _device << ").");
			return false;
		}
	} // done setting virtual/physical formats.
	// Get the stream / device latency.
	uint32_t latency;
	dataSize = sizeof(uint32_t);
	property.mSelector = kAudioDevicePropertyLatency;
	if (AudioObjectHasProperty(id, &property) == true) {
		result = AudioObjectGetPropertyData(id, &property, 0, nullptr, &dataSize, &latency);
		if (result == kAudioHardwareNoError) {
			m_latency[ _mode ] = latency;
		} else {
			ATA_ERROR("system error (" << getErrorCode(result) << ") getting device latency for device (" << _device << ").");
			return false;
		}
	}
	// Byte-swapping: According to AudioHardware.h, the stream data will
	// always be presented in native-endian format, so we should never
	// need to byte swap.
	m_doByteSwap[modeToIdTable(_mode)] = false;
	// From the CoreAudio documentation, PCM data must be supplied as
	// 32-bit floats.
	m_userFormat = _format;
	m_deviceFormat[modeToIdTable(_mode)] = FLOAT32;
	if (streamCount == 1) {
		m_nDeviceChannels[modeToIdTable(_mode)] = description.mChannelsPerFrame;
	} else {
		// multiple streams
		m_nDeviceChannels[modeToIdTable(_mode)] = _channels;
	}
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	m_channelOffset[modeToIdTable(_mode)] = channelOffset;	// offset within a CoreAudio stream
	m_deviceInterleaved[modeToIdTable(_mode)] = true;
	if (monoMode == true) {
		m_deviceInterleaved[modeToIdTable(_mode)] = false;
	}
	// Set flags for buffer conversion.
	m_doConvertBuffer[modeToIdTable(_mode)] = false;
	if (m_userFormat != m_deviceFormat[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (m_nUserChannels[modeToIdTable(_mode)] < m_nDeviceChannels[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (streamCount == 1) {
		if (    m_nUserChannels[modeToIdTable(_mode)] > 1
		     && m_deviceInterleaved[modeToIdTable(_mode)] == false) {
			m_doConvertBuffer[modeToIdTable(_mode)] = true;
		}
	} else if (monoMode) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	// Allocate our CoreHandle structure for the stream.
	CoreHandle *handle = 0;
	if (m_apiHandle == 0) {
		handle = new CoreHandle;
		if (handle == nullptr) {
			ATA_ERROR("error allocating CoreHandle memory.");
			return false;
		}
		m_apiHandle = (void *) handle;
	} else {
		handle = (CoreHandle *) m_apiHandle;
	}
	handle->iStream[modeToIdTable(_mode)] = firstStream;
	handle->nStreams[modeToIdTable(_mode)] = streamCount;
	handle->id[modeToIdTable(_mode)] = id;
	// Allocate necessary internal buffers.
	uint64_t bufferBytes;
	bufferBytes = m_nUserChannels[modeToIdTable(_mode)] * *_bufferSize * audio::getFormatBytes(m_userFormat);
	//	m_userBuffer[modeToIdTable(_mode)] = (char *) calloc(bufferBytes, 1);
	m_userBuffer[modeToIdTable(_mode)] = (char *) malloc(bufferBytes * sizeof(char));
	memset(m_userBuffer[modeToIdTable(_mode)], 0, bufferBytes * sizeof(char));
	if (m_userBuffer[modeToIdTable(_mode)] == nullptr) {
		ATA_ERROR("error allocating user buffer memory.");
		goto error;
	}
	// If possible, we will make use of the CoreAudio stream buffers as
	// "device buffers".	However, we can't do this if using multiple
	// streams.
	if (    m_doConvertBuffer[modeToIdTable(_mode)]
	     && handle->nStreams[modeToIdTable(_mode)] > 1) {
		bool makeBuffer = true;
		bufferBytes = m_nDeviceChannels[modeToIdTable(_mode)] * audio::getFormatBytes(m_deviceFormat[modeToIdTable(_mode)]);
		if (_mode == airtaudio::mode_input) {
			if (    m_mode == airtaudio::mode_output
			     && m_deviceBuffer) {
				uint64_t bytesOut = m_nDeviceChannels[0] * audio::getFormatBytes(m_deviceFormat[0]);
				if (bufferBytes <= bytesOut) {
					makeBuffer = false;
				}
			}
		}
		if (makeBuffer) {
			bufferBytes *= *_bufferSize;
			if (m_deviceBuffer) {
				free(m_deviceBuffer);
				m_deviceBuffer = nullptr;
			}
			m_deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_deviceBuffer == nullptr) {
				ATA_ERROR("error allocating device buffer memory.");
				goto error;
			}
		}
	}
	m_sampleRate = _sampleRate;
	m_device[modeToIdTable(_mode)] = _device;
	m_state = airtaudio::state_stopped;
	m_callbackInfo.object = (void *) this;
	// Setup the buffer conversion information structure.
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		if (streamCount > 1) {
			setConvertInfo(_mode, 0);
		} else {
			setConvertInfo(_mode, channelOffset);
		}
	}
	if (    _mode == airtaudio::mode_input
	     && m_mode == airtaudio::mode_output
	     && m_device[0] == _device) {
		// Only one callback procedure per device.
		m_mode = airtaudio::mode_duplex;
	} else {
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5)
		result = AudioDeviceCreateIOProcID(id, &airtaudio::api::Core::callbackEvent, (void *) &m_callbackInfo, &handle->procId[modeToIdTable(_mode)]);
#else
		// deprecated in favor of AudioDeviceCreateIOProcID()
		result = AudioDeviceAddIOProc(id, &airtaudio::api::Core::callbackEvent, (void *) &m_callbackInfo);
#endif
		if (result != noErr) {
			ATA_ERROR("system error setting callback for device (" << _device << ").");
			goto error;
		}
		if (    m_mode == airtaudio::mode_output
		     && _mode == airtaudio::mode_input) {
			m_mode = airtaudio::mode_duplex;
		} else {
			m_mode = _mode;
		}
	}
	// Setup the device property listener for over/underload.
	property.mSelector = kAudioDeviceProcessorOverload;
	result = AudioObjectAddPropertyListener(id, &property, xrunListener, (void *) handle);
	return true;
error:
	if (handle) {
		delete handle;
		m_apiHandle = 0;
	}
	for (int32_t i=0; i<2; i++) {
		if (m_userBuffer[i]) {
			free(m_userBuffer[i]);
			m_userBuffer[i] = 0;
		}
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	m_state = airtaudio::state_closed;
	return false;
}

enum airtaudio::error airtaudio::api::Core::closeStream() {
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("no open stream to close!");
		return airtaudio::error_warning;
	}
	CoreHandle *handle = (CoreHandle *) m_apiHandle;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		if (m_state == airtaudio::state_running) {
			AudioDeviceStop(handle->id[0], &airtaudio::api::Core::callbackEvent);
		}
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5)
		AudioDeviceDestroyIOProcID(handle->id[0], handle->procId[0]);
#else
		// deprecated in favor of AudioDeviceDestroyIOProcID()
		AudioDeviceRemoveIOProc(handle->id[0], &airtaudio::api::Core::callbackEvent);
#endif
	}
	if (    m_mode == airtaudio::mode_input
	     || (    m_mode == airtaudio::mode_duplex
	          && m_device[0] != m_device[1])) {
		if (m_state == airtaudio::state_running) {
			AudioDeviceStop(handle->id[1], &airtaudio::api::Core::callbackEvent);
		}
#if defined(MAC_OS_X_VERSION_10_5) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5)
		AudioDeviceDestroyIOProcID(handle->id[1], handle->procId[1]);
#else
		// deprecated in favor of AudioDeviceDestroyIOProcID()
		AudioDeviceRemoveIOProc(handle->id[1], &airtaudio::api::Core::callbackEvent);
#endif
	}
	for (int32_t i=0; i<2; i++) {
		if (m_userBuffer[i]) {
			free(m_userBuffer[i]);
			m_userBuffer[i] = nullptr;
		}
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = nullptr;
	}
	delete handle;
	m_apiHandle = 0;
	m_mode = airtaudio::mode_unknow;
	m_state = airtaudio::state_closed;
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Core::startStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_state == airtaudio::state_running) {
		ATA_ERROR("the stream is already running!");
		return airtaudio::error_warning;
	}
	OSStatus result = noErr;
	CoreHandle *handle = (CoreHandle *) m_apiHandle;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		result = AudioDeviceStart(handle->id[0], &airtaudio::api::Core::callbackEvent);
		if (result != noErr) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") starting callback procedure on device (" << m_device[0] << ").");
			goto unlock;
		}
	}
	if (    m_mode == airtaudio::mode_input
	     || (    m_mode == airtaudio::mode_duplex
	          && m_device[0] != m_device[1])) {
		result = AudioDeviceStart(handle->id[1], &airtaudio::api::Core::callbackEvent);
		if (result != noErr) {
			ATA_ERROR("system error starting input callback procedure on device (" << m_device[1] << ").");
			goto unlock;
		}
	}
	handle->drainCounter = 0;
	handle->internalDrain = false;
	m_state = airtaudio::state_running;
unlock:
	if (result == noErr) {
		return airtaudio::error_none;
	}
	return airtaudio::error_systemError;
}

enum airtaudio::error airtaudio::api::Core::stopStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return airtaudio::error_warning;
	}
	OSStatus result = noErr;
	CoreHandle *handle = (CoreHandle *) m_apiHandle;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		if (handle->drainCounter == 0) {
			std::unique_lock<std::mutex> lck(m_mutex);
			handle->drainCounter = 2;
			handle->condition.wait(lck);
		}
		result = AudioDeviceStop(handle->id[0], &airtaudio::api::Core::callbackEvent);
		if (result != noErr) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") stopping callback procedure on device (" << m_device[0] << ").");
			goto unlock;
		}
	}
	if (    m_mode == airtaudio::mode_input
	     || (    m_mode == airtaudio::mode_duplex
	          && m_device[0] != m_device[1])) {
		result = AudioDeviceStop(handle->id[1], &airtaudio::api::Core::callbackEvent);
		if (result != noErr) {
			ATA_ERROR("system error (" << getErrorCode(result) << ") stopping input callback procedure on device (" << m_device[1] << ").");
			goto unlock;
		}
	}
	m_state = airtaudio::state_stopped;
unlock:
	if (result == noErr) {
		return airtaudio::error_none;
	}
	return airtaudio::error_systemError;
}

enum airtaudio::error airtaudio::api::Core::abortStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return airtaudio::error_warning;
	}
	CoreHandle* handle = (CoreHandle*)m_apiHandle;
	handle->drainCounter = 2;
	return stopStream();
}

// This function will be called by a spawned thread when the user
// callback function signals that the stream should be stopped or
// aborted.	It is better to handle it this way because the
// callbackEvent() function probably should return before the AudioDeviceStop()
// function is called.
void airtaudio::api::Core::coreStopStream(void *_userData) {
	airtaudio::api::Core* myClass = reinterpret_cast<airtaudio::api::Core*>(_userData);
	myClass->stopStream();
}

bool airtaudio::api::Core::callbackEvent(AudioDeviceID _deviceId,
                                         const AudioBufferList *_inBufferList,
                                         const AudioBufferList *_outBufferList) {
	if (    m_state == airtaudio::state_stopped
	     || m_state == airtaudio::state_stopping) {
		return true;
	}
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("the stream is closed ... this shouldn't happen!");
		return false;
	}
	CallbackInfo *info = (CallbackInfo *) &m_callbackInfo;
	CoreHandle *handle = (CoreHandle *) m_apiHandle;
	// Check if we were draining the stream and signal is finished.
	if (handle->drainCounter > 3) {
		m_state = airtaudio::state_stopping;
		if (handle->internalDrain == true) {
			new std::thread(&airtaudio::api::Core::coreStopStream, this);
		} else {
			// external call to stopStream()
			handle->condition.notify_one();
		}
		return true;
	}
	AudioDeviceID outputDevice = handle->id[0];
	// Invoke user callback to get fresh output data UNLESS we are
	// draining stream or duplex mode AND the input/output devices are
	// different AND this function is called for the input device.
	if (handle->drainCounter == 0 && (m_mode != airtaudio::mode_duplex || _deviceId == outputDevice)) {
		double streamTime = getStreamTime();
		enum airtaudio::status status = airtaudio::status_ok;
		if (    m_mode != airtaudio::mode_input
		     && handle->xrun[0] == true) {
			status |= airtaudio::status_underflow;
			handle->xrun[0] = false;
		}
		if (    m_mode != airtaudio::mode_output
		     && handle->xrun[1] == true) {
			status |= airtaudio::mode_input_OVERFLOW;
			handle->xrun[1] = false;
		}
		int32_t cbReturnValue = info->callback(m_userBuffer[0],
		                                       m_userBuffer[1],
		                                       m_bufferSize,
		                                       streamTime,
		                                       status);
		if (cbReturnValue == 2) {
			m_state = airtaudio::state_stopping;
			handle->drainCounter = 2;
			abortStream();
			return true;
		} else if (cbReturnValue == 1) {
			handle->drainCounter = 1;
			handle->internalDrain = true;
		}
	}
	if (    m_mode == airtaudio::mode_output
	     || (    m_mode == airtaudio::mode_duplex
	          && _deviceId == outputDevice)) {
		if (handle->drainCounter > 1) {
			// write zeros to the output stream
			if (handle->nStreams[0] == 1) {
				memset(_outBufferList->mBuffers[handle->iStream[0]].mData,
				       0,
				       _outBufferList->mBuffers[handle->iStream[0]].mDataByteSize);
			} else {
				// fill multiple streams with zeros
				for (uint32_t i=0; i<handle->nStreams[0]; i++) {
					memset(_outBufferList->mBuffers[handle->iStream[0]+i].mData,
					       0,
					       _outBufferList->mBuffers[handle->iStream[0]+i].mDataByteSize);
				}
			}
		} else if (handle->nStreams[0] == 1) {
			if (m_doConvertBuffer[0]) {
				// convert directly to CoreAudio stream buffer
				convertBuffer((char*)_outBufferList->mBuffers[handle->iStream[0]].mData,
				              m_userBuffer[0],
				              m_convertInfo[0]);
			} else {
				// copy from user buffer
				memcpy(_outBufferList->mBuffers[handle->iStream[0]].mData,
				       m_userBuffer[0],
				       _outBufferList->mBuffers[handle->iStream[0]].mDataByteSize);
			}
		} else {
			// fill multiple streams
			float *inBuffer = (float *) m_userBuffer[0];
			if (m_doConvertBuffer[0]) {
				convertBuffer(m_deviceBuffer, m_userBuffer[0], m_convertInfo[0]);
				inBuffer = (float *) m_deviceBuffer;
			}
			if (m_deviceInterleaved[0] == false) { // mono mode
				uint32_t bufferBytes = _outBufferList->mBuffers[handle->iStream[0]].mDataByteSize;
				for (uint32_t i=0; i<m_nUserChannels[0]; i++) {
					memcpy(_outBufferList->mBuffers[handle->iStream[0]+i].mData,
					       (void *)&inBuffer[i*m_bufferSize],
					       bufferBytes);
				}
			} else {
				// fill multiple multi-channel streams with interleaved data
				uint32_t streamChannels, channelsLeft, inJump, outJump, inOffset;
				float *out, *in;
				bool inInterleaved = true;
				uint32_t inChannels = m_nUserChannels[0];
				if (m_doConvertBuffer[0]) {
					inInterleaved = true; // device buffer will always be interleaved for nStreams > 1 and not mono mode
					inChannels = m_nDeviceChannels[0];
				}
				if (inInterleaved) {
					inOffset = 1;
				} else {
					inOffset = m_bufferSize;
				}
				channelsLeft = inChannels;
				for (uint32_t i=0; i<handle->nStreams[0]; i++) {
					in = inBuffer;
					out = (float *) _outBufferList->mBuffers[handle->iStream[0]+i].mData;
					streamChannels = _outBufferList->mBuffers[handle->iStream[0]+i].mNumberChannels;
					outJump = 0;
					// Account for possible channel offset in first stream
					if (i == 0 && m_channelOffset[0] > 0) {
						streamChannels -= m_channelOffset[0];
						outJump = m_channelOffset[0];
						out += outJump;
					}
					// Account for possible unfilled channels at end of the last stream
					if (streamChannels > channelsLeft) {
						outJump = streamChannels - channelsLeft;
						streamChannels = channelsLeft;
					}
					// Determine input buffer offsets and skips
					if (inInterleaved) {
						inJump = inChannels;
						in += inChannels - channelsLeft;
					} else {
						inJump = 1;
						in += (inChannels - channelsLeft) * inOffset;
					}
					for (uint32_t i=0; i<m_bufferSize; i++) {
						for (uint32_t j=0; j<streamChannels; j++) {
							*out++ = in[j*inOffset];
						}
						out += outJump;
						in += inJump;
					}
					channelsLeft -= streamChannels;
				}
			}
		}
		if (handle->drainCounter) {
			handle->drainCounter++;
			goto unlock;
		}
	}
	AudioDeviceID inputDevice;
	inputDevice = handle->id[1];
	if (    m_mode == airtaudio::mode_input
	     || (    m_mode == airtaudio::mode_duplex
	          && _deviceId == inputDevice)) {
		if (handle->nStreams[1] == 1) {
			if (m_doConvertBuffer[1]) {
				// convert directly from CoreAudio stream buffer
				convertBuffer(m_userBuffer[1],
				              (char *) _inBufferList->mBuffers[handle->iStream[1]].mData,
				              m_convertInfo[1]);
			} else { // copy to user buffer
				memcpy(m_userBuffer[1],
				       _inBufferList->mBuffers[handle->iStream[1]].mData,
				       _inBufferList->mBuffers[handle->iStream[1]].mDataByteSize);
			}
		} else { // read from multiple streams
			float *outBuffer = (float *) m_userBuffer[1];
			if (m_doConvertBuffer[1]) {
				outBuffer = (float *) m_deviceBuffer;
			}
			if (m_deviceInterleaved[1] == false) {
				// mono mode
				uint32_t bufferBytes = _inBufferList->mBuffers[handle->iStream[1]].mDataByteSize;
				for (uint32_t i=0; i<m_nUserChannels[1]; i++) {
					memcpy((void *)&outBuffer[i*m_bufferSize],
					       _inBufferList->mBuffers[handle->iStream[1]+i].mData,
					       bufferBytes);
				}
			} else {
				// read from multiple multi-channel streams
				uint32_t streamChannels, channelsLeft, inJump, outJump, outOffset;
				float *out, *in;
				bool outInterleaved = true;
				uint32_t outChannels = m_nUserChannels[1];
				if (m_doConvertBuffer[1]) {
					outInterleaved = true; // device buffer will always be interleaved for nStreams > 1 and not mono mode
					outChannels = m_nDeviceChannels[1];
				}
				if (outInterleaved) {
					outOffset = 1;
				} else {
					outOffset = m_bufferSize;
				}
				channelsLeft = outChannels;
				for (uint32_t i=0; i<handle->nStreams[1]; i++) {
					out = outBuffer;
					in = (float *) _inBufferList->mBuffers[handle->iStream[1]+i].mData;
					streamChannels = _inBufferList->mBuffers[handle->iStream[1]+i].mNumberChannels;
					inJump = 0;
					// Account for possible channel offset in first stream
					if (i == 0 && m_channelOffset[1] > 0) {
						streamChannels -= m_channelOffset[1];
						inJump = m_channelOffset[1];
						in += inJump;
					}
					// Account for possible unread channels at end of the last stream
					if (streamChannels > channelsLeft) {
						inJump = streamChannels - channelsLeft;
						streamChannels = channelsLeft;
					}
					// Determine output buffer offsets and skips
					if (outInterleaved) {
						outJump = outChannels;
						out += outChannels - channelsLeft;
					} else {
						outJump = 1;
						out += (outChannels - channelsLeft) * outOffset;
					}
					for (uint32_t i=0; i<m_bufferSize; i++) {
						for (uint32_t j=0; j<streamChannels; j++) {
							out[j*outOffset] = *in++;
						}
						out += outJump;
						in += inJump;
					}
					channelsLeft -= streamChannels;
				}
			}
			if (m_doConvertBuffer[1]) { // convert from our internal "device" buffer
				convertBuffer(m_userBuffer[1],
				              m_deviceBuffer,
				              m_convertInfo[1]);
			}
		}
	}

unlock:
	//m_mutex.unlock();
	airtaudio::Api::tickStreamTime();
	return true;
}

const char* airtaudio::api::Core::getErrorCode(OSStatus _code) {
	switch(_code) {
		case kAudioHardwareNotRunningError:
			return "kAudioHardwareNotRunningError";
		case kAudioHardwareUnspecifiedError:
			return "kAudioHardwareUnspecifiedError";
		case kAudioHardwareUnknownPropertyError:
			return "kAudioHardwareUnknownPropertyError";
		case kAudioHardwareBadPropertySizeError:
			return "kAudioHardwareBadPropertySizeError";
		case kAudioHardwareIllegalOperationError:
			return "kAudioHardwareIllegalOperationError";
		case kAudioHardwareBadObjectError:
			return "kAudioHardwareBadObjectError";
		case kAudioHardwareBadDeviceError:
			return "kAudioHardwareBadDeviceError";
		case kAudioHardwareBadStreamError:
			return "kAudioHardwareBadStreamError";
		case kAudioHardwareUnsupportedOperationError:
			return "kAudioHardwareUnsupportedOperationError";
		case kAudioDeviceUnsupportedFormatError:
			return "kAudioDeviceUnsupportedFormatError";
		case kAudioDevicePermissionsError:
			return "kAudioDevicePermissionsError";
		default:
			return "CoreAudio unknown error";
	}
}

#endif
