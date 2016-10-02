/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifdef ORCHESTRA_BUILD_JAVA

//#include <ewol/context/Context.h>
#include <unistd.h>
#include <audio/orchestra/Interface.hpp>
#include <audio/orchestra/debug.hpp>
#include <audio/orchestra/api/AndroidNativeInterface.hpp>
#include <audio/orchestra/api/Android.hpp>
#include <climits>

ememory::SharedPtr<audio::orchestra::Api> audio::orchestra::api::Android::create() {
	ATA_INFO("Create Android device ... ");
	return ememory::SharedPtr<audio::orchestra::api::Android>(new audio::orchestra::api::Android());
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
	// Can not close the stream now...
	return audio::orchestra::api::android::stopStream(m_uid);
}

enum audio::orchestra::error audio::orchestra::api::Android::abortStream() {
	ATA_INFO("Abort Stream");
	// Can not close the stream now...
	return audio::orchestra::error_none;
}


void audio::orchestra::api::Android::playback(int16_t* _dst, int32_t _nbChunk) {
	// clear output buffer:
	if (_dst != nullptr) {
		memset(_dst, 0, _nbChunk*audio::getFormatBytes(m_deviceFormat[modeToIdTable(m_mode)])*m_nDeviceChannels[modeToIdTable(m_mode)]);
	}
	int32_t doStopStream = 0;
	audio::Time streamTime = getStreamTime();
	std::vector<enum audio::orchestra::status> status;
	if (m_doConvertBuffer[modeToIdTable(m_mode)] == true) {
		ATA_VERBOSE("Need playback data " << int32_t(_nbChunk) << " userbuffer size = " << m_userBuffer[audio::orchestra::mode_output].size() << "pointer=" << int64_t(&m_userBuffer[audio::orchestra::mode_output][0]));
		doStopStream = m_callback(nullptr,
		                          audio::Time(),
		                          &m_userBuffer[m_mode][0],
		                          streamTime,
		                          uint32_t(_nbChunk),
		                          status);
		convertBuffer((char*)_dst, (char*)&m_userBuffer[audio::orchestra::mode_output][0], m_convertInfo[audio::orchestra::mode_output]);
	} else {
		ATA_VERBOSE("Need playback data " << int32_t(_nbChunk) << " pointer=" << int64_t(_dst));
		doStopStream = m_callback(nullptr,
		                          audio::Time(),
		                          _dst,
		                          streamTime,
		                          uint32_t(_nbChunk),
		                          status);
		
	}
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	audio::orchestra::Api::tickStreamTime();
}

void audio::orchestra::api::Android::record(int16_t* _dst, int32_t _nbChunk) {
	int32_t doStopStream = 0;
	audio::Time streamTime = getStreamTime();
	std::vector<enum audio::orchestra::status> status;
	if (m_doConvertBuffer[modeToIdTable(m_mode)] == true) {
		ATA_VERBOSE("Need playback data " << int32_t(_nbChunk) << " userbuffer size = " << m_userBuffer[audio::orchestra::mode_output].size() << "pointer=" << int64_t(&m_userBuffer[audio::orchestra::mode_output][0]));
		convertBuffer((char*)&m_userBuffer[audio::orchestra::mode_input][0], (char*)_dst, m_convertInfo[audio::orchestra::mode_input]);
		doStopStream = m_callback(&m_userBuffer[m_mode][0],
		                          streamTime,
		                          nullptr,
		                          audio::Time(),
		                          uint32_t(_nbChunk),
		                          status);
	} else {
		ATA_VERBOSE("Need playback data " << int32_t(_nbChunk) << " pointer=" << int64_t(_dst));
		doStopStream = m_callback(_dst,
		                          streamTime,
		                          nullptr,
		                          audio::Time(),
		                          uint32_t(_nbChunk),
		                          status);
		
	}
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	audio::orchestra::Api::tickStreamTime();
}

bool audio::orchestra::api::Android::open(uint32_t _device,
                                          audio::orchestra::mode _mode,
                                          uint32_t _channels,
                                          uint32_t _firstChannel,
                                          uint32_t _sampleRate,
                                          audio::format _format,
                                          uint32_t *_bufferSize,
                                          const audio::orchestra::StreamOptions& _options) {
	bool ret = false;
	ATA_INFO("Probe : device=" << _device << " channels=" << _channels << " firstChannel=" << _firstChannel << " sampleRate=" << _sampleRate);
	m_mode = _mode;
	m_userFormat = _format;
	m_nUserChannels[modeToIdTable(m_mode)] = _channels;
	
	m_uid = audio::orchestra::api::android::open(_device, m_mode, _channels, _firstChannel, _sampleRate, _format, _bufferSize, _options, ememory::staticPointerCast<audio::orchestra::api::Android>(sharedFromThis()));
	if (m_uid < 0) {
		ret = false;
	} else {
		ret = true;
	}
	m_bufferSize = 256;
	m_sampleRate = _sampleRate;
	m_doByteSwap[modeToIdTable(m_mode)] = false; // for endienness ...
	
	// TODO : For now, we write it in hard ==> to bu update later ...
	m_deviceFormat[modeToIdTable(m_mode)] = audio::format_int16;
	m_nDeviceChannels[modeToIdTable(m_mode)] = 2;
	m_deviceInterleaved[modeToIdTable(m_mode)] = true;
	
	m_doConvertBuffer[modeToIdTable(m_mode)] = false;
	if (m_userFormat != m_deviceFormat[modeToIdTable(m_mode)]) {
		m_doConvertBuffer[modeToIdTable(m_mode)] = true;
	}
	if (m_nUserChannels[modeToIdTable(m_mode)] < m_nDeviceChannels[modeToIdTable(m_mode)]) {
		m_doConvertBuffer[modeToIdTable(m_mode)] = true;
	}
	if (    m_deviceInterleaved[modeToIdTable(m_mode)] == false
	     && m_nUserChannels[modeToIdTable(m_mode)] > 1) {
		m_doConvertBuffer[modeToIdTable(m_mode)] = true;
	}
	if (m_doConvertBuffer[modeToIdTable(m_mode)] == true) {
		// Allocate necessary internal buffers.
		uint64_t bufferBytes = m_nUserChannels[modeToIdTable(m_mode)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		m_userBuffer[modeToIdTable(m_mode)].resize(bufferBytes);
		if (m_userBuffer[modeToIdTable(m_mode)].size() == 0) {
			ATA_ERROR("error allocating user buffer memory.");
		}
		setConvertInfo(m_mode, _firstChannel);
	}
	ATA_INFO("device format : " << m_deviceFormat[modeToIdTable(m_mode)] << " user format : " << m_userFormat);
	ATA_INFO("device channels : " << m_nDeviceChannels[modeToIdTable(m_mode)] << " user channels : " << m_nUserChannels[modeToIdTable(m_mode)]);
	ATA_INFO("do convert buffer : " << m_doConvertBuffer[modeToIdTable(m_mode)]);
	if (ret == false) {
		ATA_ERROR("Can not open device.");
	}
	return ret;
}

#endif

