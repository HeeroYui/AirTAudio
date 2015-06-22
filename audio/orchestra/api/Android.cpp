/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifdef ORCHESTRA_BUILD_JAVA

//#include <ewol/context/Context.h>
#include <unistd.h>
#include <audio/orchestra/Interface.h>
#include <audio/orchestra/debug.h>
#include <audio/orchestra/api/AndroidNativeInterface.h>
#include <limits.h>

#undef __class__
#define __class__ "api::Android"

audio::orchestra::Api* audio::orchestra::api::Android::create() {
	ATA_INFO("Create Android device ... ");
	return new audio::orchestra::api::Android();
}


audio::orchestra::api::Android::Android() {
	ATA_INFO("Create Android interface");
}

audio::orchestra::api::Android::~Android() {
	ATA_INFO("Destroy Android interface");
}

uint32_t audio::orchestra::api::Android::getDeviceCount() {
	//ATA_INFO("Get device count:"<< m_devices.size());
	return audio::orchestra::api::android::getDeviceCount();
}

audio::orchestra::DeviceInfo audio::orchestra::api::Android::getDeviceInfo(uint32_t _device) {
	//ATA_INFO("Get device info ...");
	return audio::orchestra::api::android::getDeviceInfo(_device);
}

enum audio::orchestra::error audio::orchestra::api::Android::closeStream() {
	ATA_INFO("Close Stream");
	// Can not close the stream now...
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Android::startStream() {
	ATA_INFO("Start Stream");
	// TODO : Check return ...
	audio::orchestra::Api::startStream();
	// Can not close the stream now...
	return audio::orchestra::api::android::startStream(0);
}

enum audio::orchestra::error audio::orchestra::api::Android::stopStream() {
	ATA_INFO("Stop stream");
	#if 0
	ewol::Context& tmpContext = ewol::getContext();
	tmpContext.audioCloseDevice(0);
	#endif
	// Can not close the stream now...
	return audio::orchestra::api::android::stopStream(0);
}

enum audio::orchestra::error audio::orchestra::api::Android::abortStream() {
	ATA_INFO("Abort Stream");
	#if 0
	ewol::Context& tmpContext = ewol::getContext();
	tmpContext.audioCloseDevice(0);
	#endif
	// Can not close the stream now...
	return audio::orchestra::error_none;
}

void audio::orchestra::api::Android::callBackEvent(void* _data,
                                                   int32_t _frameRate) {
	#if 0
	int32_t doStopStream = 0;
	audio::Time streamTime = getStreamTime();
	std::vector<enum audio::orchestra::status> status;
	if (m_doConvertBuffer[audio::orchestra::mode_output] == true) {
		doStopStream = m_callback(nullptr,
		                          audio::Time(),
		                          m_userBuffer[audio::orchestra::mode_output],
		                          streamTime,
		                          _frameRate,
		                          status);
		convertBuffer((char*)_data, (char*)m_userBuffer[audio::orchestra::mode_output], m_convertInfo[audio::orchestra::mode_output]);
	} else {
		doStopStream = m_callback(_data,
		                          streamTime,
		                          nullptr,
		                          audio::Time(),
		                          _frameRate,
		                          status);
	}
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	audio::orchestra::Api::tickStreamTime();
	#endif
}

void audio::orchestra::api::Android::androidCallBackEvent(void* _data,
                                                          int32_t _frameRate,
                                                          void* _userData) {
	#if 0
	if (_userData == nullptr) {
		ATA_INFO("callback event ... nullptr pointer");
		return;
	}
	audio::orchestra::api::Android* myClass = static_cast<audio::orchestra::api::Android*>(_userData);
	myClass->callBackEvent(_data, _frameRate/2);
	#endif
}

bool audio::orchestra::api::Android::probeDeviceOpen(uint32_t _device,
                                                     audio::orchestra::mode _mode,
                                                     uint32_t _channels,
                                                     uint32_t _firstChannel,
                                                     uint32_t _sampleRate,
                                                     audio::format _format,
                                                     uint32_t *_bufferSize,
                                                     const audio::orchestra::StreamOptions& _options) {
	bool ret = false;
	ATA_INFO("Probe : device=" << _device << " channels=" << _channels << " firstChannel=" << _firstChannel << " sampleRate=" << _sampleRate);
	ret = audio::orchestra::api::android::open(_device, _mode, _channels, _firstChannel, _sampleRate, _format, _bufferSize, _options);
	#if 0
	if (_mode != audio::orchestra::mode_output) {
		ATA_ERROR("Can not start a device input or duplex for Android ...");
		return false;
	}
	m_userFormat = _format;
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	ewol::Context& tmpContext = ewol::getContext();
	if (_format == SINT8) {
		ret = tmpContext.audioOpenDevice(_device, _sampleRate, _channels, 0, androidCallBackEvent, this);
	} else {
		ret = tmpContext.audioOpenDevice(_device, _sampleRate, _channels, 1, androidCallBackEvent, this);
	}
	m_bufferSize = 256;
	m_sampleRate = _sampleRate;
	m_doByteSwap[modeToIdTable(_mode)] = false; // for endienness ...
	
	// TODO : For now, we write it in hard ==> to bu update later ...
	m_deviceFormat[modeToIdTable(_mode)] = SINT16;
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
		m_userBuffer[modeToIdTable(_mode)] = (char *) calloc(bufferBytes, 1);
		if (m_userBuffer[modeToIdTable(_mode)] == nullptr) {
			ATA_ERROR("audio::orchestra::api::Android::probeDeviceOpen: error allocating user buffer memory.");
		}
		setConvertInfo(_mode, _firstChannel);
	}
	ATA_INFO("device format : " << m_deviceFormat[modeToIdTable(_mode)] << " user format : " << m_userFormat);
	ATA_INFO("device channels : " << m_nDeviceChannels[modeToIdTable(_mode)] << " user channels : " << m_nUserChannels[modeToIdTable(_mode)]);
	ATA_INFO("do convert buffer : " << m_doConvertBuffer[modeToIdTable(_mode)]);
	if (ret == false) {
		ATA_ERROR("Can not open device.");
	}
	#endif
	return ret;
}

#endif

