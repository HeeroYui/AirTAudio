/**
 * @author Edouard DUPIN
 * 
 * @license like MIT (see license file)
 */

#ifdef __IOS_CORE__

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>

#include <unistd.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <limits.h>

#undef __class__
#define __class__ "api::CoreIos"

airtaudio::Api* airtaudio::api::CoreIos::Create(void) {
	ATA_INFO("Create CoreIos device ... ");
	return new airtaudio::api::CoreIos();
}

#define kOutputBus 0
#define kInputBus 1

namespace airtaudio {
	namespace api {
		class CoreIosPrivate {
			public:
				AudioComponentInstance audioUnit;
		};
	};
};



airtaudio::api::CoreIos::CoreIos(void) :
  m_private(new airtaudio::api::CoreIosPrivate) {
	ATA_INFO("new CoreIos");
	int32_t deviceCount = 2;
	ATA_ERROR("Get count devices : " << 2);
	airtaudio::DeviceInfo tmp;
	// Add default output format :
	tmp.name = "out";
	tmp.sampleRates.push_back(48000);
	tmp.outputChannels = 2;
	tmp.inputChannels = 0;
	tmp.duplexChannels = 0;
	tmp.isDefaultOutput = true;
	tmp.isDefaultInput = false;
	tmp.nativeFormats.push_back(audio::format_int16);
	m_devices.push_back(tmp);
	// add default input format:
	tmp.name = "in";
	tmp.sampleRates.push_back(48000);
	tmp.outputChannels = 0;
	tmp.inputChannels = 2;
	tmp.duplexChannels = 0;
	tmp.isDefaultOutput = false;
	tmp.isDefaultInput = true;
	tmp.nativeFormats.push_back(audio::format_int16);
	m_devices.push_back(tmp);
	ATA_INFO("Create CoreIOs interface (end)");
}

airtaudio::api::CoreIos::~CoreIos(void) {
	ATA_INFO("Destroy CoreIOs interface");
	AudioUnitUninitialize(m_private->audioUnit);
	delete m_private;
	m_private = nullptr;
}

uint32_t airtaudio::api::CoreIos::getDeviceCount(void) {
	//ATA_INFO("Get device count:"<< m_devices.size());
	return m_devices.size();
}

airtaudio::DeviceInfo airtaudio::api::CoreIos::getDeviceInfo(uint32_t _device) {
	//ATA_INFO("Get device info ...");
	return m_devices[_device];
}

enum airtaudio::errorType airtaudio::api::CoreIos::closeStream(void) {
	ATA_INFO("Close Stream");
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::CoreIos::startStream(void) {
	ATA_INFO("Start Stream");
	OSStatus status = AudioOutputUnitStart(m_private->audioUnit);
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::CoreIos::stopStream(void) {
	ATA_INFO("Stop stream");
	OSStatus status = AudioOutputUnitStop(m_private->audioUnit);
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::CoreIos::abortStream(void) {
	ATA_INFO("Abort Stream");
	OSStatus status = AudioOutputUnitStop(m_private->audioUnit);
	// Can not close the stream now...
	return airtaudio::errorNone;
}

void airtaudio::api::CoreIos::callBackEvent(void* _data,
                                            int32_t _frameRate) {
	
	#if 0
	static double value=0;
	int16_t* vals = (int16_t*)_data;
	for (int32_t iii=0; iii<_frameRate; ++iii) {
		*vals++ = (int16_t)(sin(value) * 32760.0);
		*vals++ = (int16_t)(sin(value) * 32760.0);
		value += 0.09;
		if (value >= M_PI*2.0) {
			value -= M_PI*2.0;
		}
	}
	return;
	#endif
	int32_t doStopStream = 0;
	double streamTime = getStreamTime();
	airtaudio::streamStatus status = 0;
	if (m_stream.doConvertBuffer[OUTPUT] == true) {
		doStopStream = m_stream.callbackInfo.callback(m_stream.userBuffer[OUTPUT],
		                                              nullptr,
		                                              _frameRate,
		                                              streamTime,
		                                              status);
		convertBuffer((char*)_data, (char*)m_stream.userBuffer[OUTPUT], m_stream.convertInfo[OUTPUT]);
	} else {
		doStopStream = m_stream.callbackInfo.callback(_data,
		                                              nullptr,
		                                              _frameRate,
		                                              streamTime,
		                                              status);
	}
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	airtaudio::Api::tickStreamTime();
}

static OSStatus playbackCallback(void *_userData, 
								 AudioUnitRenderActionFlags *ioActionFlags, 
								 const AudioTimeStamp *inTimeStamp, 
								 uint32_t inBusNumber, 
								 uint32_t inNumberFrames, 
								 AudioBufferList *ioData) {
	if (_userData == nullptr) {
		ATA_ERROR("callback event ... nullptr pointer");
		return -1;
	}
	airtaudio::api::CoreIos* myClass = static_cast<airtaudio::api::CoreIos*>(_userData);
	// get all requested buffer :
	for (int32_t iii=0; iii < ioData->mNumberBuffers; iii++) {
		AudioBuffer buffer = ioData->mBuffers[iii];
		int32_t numberFrame =  buffer.mDataByteSize/2/*stereo*/ /sizeof(int16_t);
		ATA_VERBOSE("request data size: " << numberFrame << " busNumber=" << inBusNumber);
		myClass->callBackEvent(buffer.mData, numberFrame);
	}
    return noErr;
}


bool airtaudio::api::CoreIos::probeDeviceOpen(uint32_t _device,
                                              airtaudio::api::StreamMode _mode,
                                              uint32_t _channels,
                                              uint32_t _firstChannel,
                                              uint32_t _sampleRate,
                                              audio::format _format,
                                              uint32_t *_bufferSize,
                                              airtaudio::StreamOptions *_options) {
	ATA_INFO("Probe : device=" << _device << " channels=" << _channels << " firstChannel=" << _firstChannel << " sampleRate=" << _sampleRate);
	if (_mode != OUTPUT) {
		ATA_ERROR("Can not start a device input or duplex for CoreIos ...");
		return false;
	}
	bool ret = true;
	
	// configure Airtaudio internal configuration:
	m_stream.userFormat = _format;
	m_stream.nUserChannels[_mode] = _channels;
	m_stream.bufferSize = 8192;
	m_stream.sampleRate = _sampleRate;
	m_stream.doByteSwap[_mode] = false; // for endienness ...
	
	// TODO : For now, we write it in hard ==> to be update later ...
	m_stream.deviceFormat[_mode] = SINT16;
	m_stream.nDeviceChannels[_mode] = 2;
	m_stream.deviceInterleaved[_mode] =	true;
	
	m_stream.doConvertBuffer[_mode] = false;
	if (m_stream.userFormat != m_stream.deviceFormat[_mode]) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (m_stream.nUserChannels[_mode] < m_stream.nDeviceChannels[_mode]) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (    m_stream.userInterleaved != m_stream.deviceInterleaved[_mode]
		&& m_stream.nUserChannels[_mode] > 1) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (m_stream.doConvertBuffer[_mode] == true) {
		// Allocate necessary internal buffers.
		uint64_t bufferBytes = m_stream.nUserChannels[_mode] * m_stream.bufferSize * formatBytes(m_stream.userFormat);
		m_stream.userBuffer[_mode] = (char *) calloc(bufferBytes, 1);
		if (m_stream.userBuffer[_mode] == nullptr) {
			ATA_ERROR("error allocating user buffer memory.");
		}
		setConvertInfo(_mode, _firstChannel);
	}
	ATA_INFO("device format : " << m_stream.deviceFormat[_mode] << " user format : " << m_stream.userFormat);
	ATA_INFO("device channels : " << m_stream.nDeviceChannels[_mode] << " user channels : " << m_stream.nUserChannels[_mode]);
	ATA_INFO("do convert buffer : " << m_stream.doConvertBuffer[_mode]);
	if (ret == false) {
		ATA_ERROR("Can not open device.");
	}
	
	// Configure IOs interface:
	OSStatus status;
	
	// Describe audio component
	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	
	// Get component
	AudioComponent inputComponent = AudioComponentFindNext(nullptr, &desc);
	
	// Get audio units
	status = AudioComponentInstanceNew(inputComponent, &m_private->audioUnit);
	if (status != 0) {
		ATA_ERROR("can not create an audio intance...");
	}
	
	uint32_t flag = 1;
	// Enable IO for playback
	status = AudioUnitSetProperty(m_private->audioUnit, 
								  kAudioOutputUnitProperty_EnableIO, 
								  kAudioUnitScope_Output, 
								  kOutputBus,
								  &flag, 
								  sizeof(flag));
	if (status != 0) {
		ATA_ERROR("can not request audio autorisation...");
	}
	
	// Describe format
	AudioStreamBasicDescription audioFormat;
	audioFormat.mSampleRate = 48000.00;
	audioFormat.mFormatID = kAudioFormatLinearPCM;
	audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	audioFormat.mFramesPerPacket = 1; //
	audioFormat.mChannelsPerFrame = 2; // stereo
	audioFormat.mBitsPerChannel = sizeof(short) * 8;
	audioFormat.mBytesPerPacket = sizeof(short) * audioFormat.mChannelsPerFrame;
	audioFormat.mBytesPerFrame = sizeof(short) * audioFormat.mChannelsPerFrame;
	audioFormat.mReserved = 0;
	// Apply format
	status = AudioUnitSetProperty(m_private->audioUnit, 
								  kAudioUnitProperty_StreamFormat, 
								  kAudioUnitScope_Input, 
								  kOutputBus, 
								  &audioFormat, 
								  sizeof(audioFormat));
	if (status != 0) {
		ATA_ERROR("can not set stream properties...");
	}
	
	
	// Set output callback
	AURenderCallbackStruct callbackStruct;
	callbackStruct.inputProc = playbackCallback;
	callbackStruct.inputProcRefCon = this;
	status = AudioUnitSetProperty(m_private->audioUnit, 
								  kAudioUnitProperty_SetRenderCallback, 
								  kAudioUnitScope_Global, 
								  kOutputBus,
								  &callbackStruct, 
								  sizeof(callbackStruct));
	if (status != 0) {
		ATA_ERROR("can not set Callback...");
	}
	
	// Initialise
	status = AudioUnitInitialize(m_private->audioUnit);
	if (status != 0) {
		ATA_ERROR("can not initialize...");
	}
	return ret;
}

#endif

