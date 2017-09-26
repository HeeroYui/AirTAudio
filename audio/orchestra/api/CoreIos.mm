/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifdef ORCHESTRA_BUILD_IOS_CORE

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>


#include <audio/orchestra/Interface.hpp>
#include <audio/orchestra/debug.hpp>
extern "C" {
	#include <limits.h>
}
#include <audio/orchestra/api/CoreIos.hpp>

ememory::SharedPtr<audio::orchestra::Api> audio::orchestra::api::CoreIos::create() {
	ATA_INFO("Create CoreIos device ... ");
	return ememory::SharedPtr<audio::orchestra::api::CoreIos>(new audio::orchestra::api::CoreIos());
}

#define kOutputBus 0
#define kInputBus 1

namespace audio {
	namespace orchestra {
		namespace api {
			class CoreIosPrivate {
				public:
					AudioComponentInstance audioUnit;
			};
		}
	}
}



audio::orchestra::api::CoreIos::CoreIos() :
  m_private(new audio::orchestra::api::CoreIosPrivate()) {
	ATA_INFO("new CoreIos");
	int32_t deviceCount = 2;
	ATA_ERROR("Get count devices : " << 2);
	audio::orchestra::DeviceInfo tmp;
	// Add default output format :
	tmp.name = "speaker";
	tmp.input = false;
	tmp.sampleRates.push_back(48000);
	tmp.channels.push_back(audio::channel_frontRight);
	tmp.channels.push_back(audio::channel_frontLeft);
	tmp.nativeFormats.push_back(audio::format_int16);
	tmp.isDefault = true;
	tmp.isCorrect = true;
	m_devices.push_back(tmp);
	// add default input format:
	tmp.name = "microphone";
	tmp.input = true;
	tmp.sampleRates.push_back(48000);
	tmp.channels.push_back(audio::channel_frontRight);
	tmp.channels.push_back(audio::channel_frontLeft);
	tmp.nativeFormats.push_back(audio::format_int16);
	tmp.isDefault = true;
	tmp.isCorrect = true;
	m_devices.push_back(tmp);
	ATA_INFO("Create CoreIOs interface (end)");
}

uint32_t audio::orchestra::api::CoreIos::getDefaultInputDevice() {
	// Should be implemented in subclasses if possible.
	return 1;
}

uint32_t audio::orchestra::api::CoreIos::getDefaultOutputDevice() {
	// Should be implemented in subclasses if possible.
	return 0;
}

audio::orchestra::api::CoreIos::~CoreIos() {
	ATA_INFO("Destroy CoreIOs interface");
	AudioUnitUninitialize(m_private->audioUnit);
}

uint32_t audio::orchestra::api::CoreIos::getDeviceCount() {
	//ATA_INFO("Get device count:"<< m_devices.size());
	return m_devices.size();
}

audio::orchestra::DeviceInfo audio::orchestra::api::CoreIos::getDeviceInfo(uint32_t _device) {
	//ATA_INFO("Get device info ...");
	if (_device >= m_devices.size()) {
		audio::orchestra::DeviceInfo tmp;
		tmp.sampleRates.push_back(0);
		tmp.channels.push_back(audio::channel_frontCenter);
		tmp.isDefault = false;
		tmp.nativeFormats.push_back(audio::format_int8);
		return tmp;
	}
	return m_devices[_device];
}

enum audio::orchestra::error audio::orchestra::api::CoreIos::closeStream() {
	ATA_INFO("Close Stream");
	// Can not close the stream now...
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::CoreIos::startStream() {
	ATA_INFO("Start Stream");
	// TODO : Check return ...
	audio::orchestra::Api::startStream();
	OSStatus status = AudioOutputUnitStart(m_private->audioUnit);
	// Can not close the stream now...
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::CoreIos::stopStream() {
	ATA_INFO("Stop stream");
	OSStatus status = AudioOutputUnitStop(m_private->audioUnit);
	// Can not close the stream now...
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::CoreIos::abortStream() {
	ATA_INFO("Abort Stream");
	OSStatus status = AudioOutputUnitStop(m_private->audioUnit);
	// Can not close the stream now...
	return audio::orchestra::error_none;
}

void audio::orchestra::api::CoreIos::callBackEvent(void* _data,
                                                   int32_t _nbChunk,
                                                   const audio::Time& _time) {
	int32_t doStopStream = 0;
	etk::Vector<enum audio::orchestra::status> status;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		if (m_doConvertBuffer[modeToIdTable(audio::orchestra::mode_output)] == true) {
			ATA_INFO("get output DATA : " << uint64_t(&m_userBuffer[modeToIdTable(audio::orchestra::mode_output)][0]));
			doStopStream = m_callback(nullptr,
			                          audio::Time(),
			                          &m_userBuffer[modeToIdTable(audio::orchestra::mode_output)][0],
			                          _time,
			                          _nbChunk,
			                          status);
			convertBuffer((char*)_data, &m_userBuffer[modeToIdTable(audio::orchestra::mode_output)][0], m_convertInfo[modeToIdTable(audio::orchestra::mode_output)]);
		} else {
			ATA_INFO("have output DATA : " << uint64_t(_data));
			doStopStream = m_callback(nullptr,
			                          _time,
			                          _data,
			                          audio::Time(),
			                          _nbChunk,
			                          status);
		}
	}
	if (    m_mode == audio::orchestra::mode_input
	     || m_mode == audio::orchestra::mode_duplex) {
		ATA_INFO("have input DATA : " << uint64_t(_data));
		doStopStream = m_callback(_data,
		                          _time,
		                          nullptr,
		                          audio::Time(),
		                          _nbChunk,
		                          status);
	}
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	audio::orchestra::Api::tickStreamTime();
}


static OSStatus playbackCallback(void *_userData,
                                 AudioUnitRenderActionFlags* _ioActionFlags,
                                 const AudioTimeStamp* _inTime,
                                 uint32_t _inBusNumber,
                                 uint32_t _inNumberFrames,
                                 AudioBufferList* _ioData) {
	if (_userData == nullptr) {
		ATA_ERROR("callback event ... nullptr pointer");
		return -1;
	}
	audio::Time tmpTimeime;
	if (_inTime != nullptr) {
		tmpTimeime = audio::Time(_inTime->mHostTime/1000000000LL, _inTime->mHostTime%1000000000LL);
	}
	audio::orchestra::api::CoreIos* myClass = static_cast<audio::orchestra::api::CoreIos*>(_userData);
	// get all requested buffer :
	for (int32_t iii=0; iii < _ioData->mNumberBuffers; iii++) {
		AudioBuffer buffer = _ioData->mBuffers[iii];
		int32_t numberFrame = buffer.mDataByteSize/2/*stereo*/ /sizeof(int16_t);
		ATA_INFO("request data size: " << numberFrame << " busNumber=" << _inBusNumber);
		myClass->callBackEvent(buffer.mData, numberFrame, tmpTimeime);
	}
	return noErr;
}


bool audio::orchestra::api::CoreIos::open(uint32_t _device,
                                          audio::orchestra::mode _mode,
                                          uint32_t _channels,
                                          uint32_t _firstChannel,
                                          uint32_t _sampleRate,
                                          audio::format _format,
                                          uint32_t *_bufferSize,
                                          const audio::orchestra::StreamOptions& _options) {
	ATA_INFO("Probe : device=" << _device << " channels=" << _channels << " firstChannel=" << _firstChannel << " sampleRate=" << _sampleRate);
	if (_mode != audio::orchestra::mode_output) {
		ATA_ERROR("Can not start a device input or duplex for CoreIos ...");
		return false;
	}
	bool ret = true;
	// TODO : This is a bad ack ....
	m_mode = audio::orchestra::mode_output;
	// configure Airtaudio internal configuration:
	m_userFormat = _format;
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	m_bufferSize = 8192;
	m_sampleRate = _sampleRate;
	m_doByteSwap[modeToIdTable(_mode)] = false; // for endienness ...
	
	// TODO : For now, we write it in hard ==> to be update later ...
	m_deviceFormat[modeToIdTable(_mode)] = audio::format_int16;
	m_nDeviceChannels[modeToIdTable(_mode)] = 2;
	m_deviceInterleaved[modeToIdTable(_mode)] =	true;
	
	m_doConvertBuffer[modeToIdTable(_mode)] = false;
	if (m_userFormat != m_deviceFormat[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (m_nUserChannels[modeToIdTable(_mode)] < m_nDeviceChannels[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (    m_deviceInterleaved[modeToIdTable(_mode)] == false
	     && m_nUserChannels[modeToIdTable(_mode)] > 1) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (m_doConvertBuffer[modeToIdTable(_mode)] == true) {
		// Allocate necessary internal buffers.
		uint64_t bufferBytes = m_nUserChannels[modeToIdTable(_mode)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		m_userBuffer[modeToIdTable(_mode)].resize(bufferBytes, 0);
		if (m_userBuffer[modeToIdTable(_mode)].size() == 0) {
			ATA_ERROR("error allocating user buffer memory.");
		}
		setConvertInfo(_mode, _firstChannel);
	}
	ATA_INFO("device format : " << m_deviceFormat[modeToIdTable(_mode)] << " user format : " << m_userFormat);
	ATA_INFO("device channels : " << m_nDeviceChannels[modeToIdTable(_mode)] << " user channels : " << m_nUserChannels[modeToIdTable(_mode)]);
	ATA_INFO("do convert buffer : " << m_doConvertBuffer[modeToIdTable(_mode)]);
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
	callbackStruct.inputProc = &playbackCallback;
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

