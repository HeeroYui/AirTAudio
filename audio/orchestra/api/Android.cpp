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

std::shared_ptr<audio::orchestra::Api> audio::orchestra::api::Android::create() {
	ATA_INFO("Create Android device ... ");
	return std::shared_ptr<audio::orchestra::Api>(new audio::orchestra::api::Android());
}


audio::orchestra::api::Android::Android() :
	m_uid(-1) {
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
	return audio::orchestra::api::android::startStream(m_uid);
}

enum audio::orchestra::error audio::orchestra::api::Android::stopStream() {
	ATA_INFO("Stop stream");
	#if 0
	ewol::Context& tmpContext = ewol::getContext();
	tmpContext.audioCloseDevice(0);
	#endif
	// Can not close the stream now...
	return audio::orchestra::api::android::stopStream(m_uid);
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


void audio::orchestra::api::Android::playback(int16_t* _dst, int32_t _nbChunk) {
	int32_t doStopStream = 0;
	audio::Time streamTime = getStreamTime();
	std::vector<enum audio::orchestra::status> status;
	ATA_INFO("Need playback data " << int32_t(_nbChunk));
	doStopStream = m_callback(nullptr,
	                          audio::Time(),
	                          &m_userBuffer[audio::orchestra::mode_output][0],
	                          streamTime,
	                          uint32_t(_nbChunk),
	                          status);
	convertBuffer((char*)_dst, (char*)&m_userBuffer[audio::orchestra::mode_output][0], m_convertInfo[audio::orchestra::mode_output]);
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	audio::orchestra::Api::tickStreamTime();
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
	
	if (_mode != audio::orchestra::mode_output) {
		ATA_ERROR("Can not start a device input or duplex for Android ...");
		return false;
	}
	m_userFormat = _format;
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	
	m_uid = audio::orchestra::api::android::open(_device, _mode, _channels, _firstChannel, _sampleRate, _format, _bufferSize, _options, std11::static_pointer_cast<audio::orchestra::api::Android>(shared_from_this()));
	if (m_uid < 0) {
		ret = false;
	} else {
		ret = true;
	}
	m_bufferSize = 256;
	m_sampleRate = _sampleRate;
	m_doByteSwap[modeToIdTable(_mode)] = false; // for endienness ...
	
	// TODO : For now, we write it in hard ==> to bu update later ...
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
		m_userBuffer[modeToIdTable(_mode)].resize(bufferBytes);
		if (m_userBuffer[modeToIdTable(_mode)].size() == 0) {
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
	return ret;
}

#endif

