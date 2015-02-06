/**
 * @author Edouard DUPIN
 * 
 * @license like MIT (see license file)
 */

#ifdef __ANDROID_JAVA__

#include <ewol/context/Context.h>
#include <unistd.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <limits.h>

#undef __class__
#define __class__ "api::Android"

airtaudio::Api* airtaudio::api::Android::Create() {
	ATA_INFO("Create Android device ... ");
	return new airtaudio::api::Android();
}


airtaudio::api::Android::Android() {
	ATA_INFO("new Android");
	// On android, we set a static device ...
	ATA_INFO("get context");
	ewol::Context& tmpContext = ewol::getContext();
	ATA_INFO("done p=" << (int64_t)&tmpContext);
	int32_t deviceCount = tmpContext.audioGetDeviceCount();
	ATA_ERROR("Get count devices : " << deviceCount);
	for (int32_t iii=0; iii<deviceCount; ++iii) {
		std::string property = tmpContext.audioGetDeviceProperty(iii);
		ATA_ERROR("Get devices property : " << property);
		std::vector<std::string> listProperty = etk::split(property, ':');
		airtaudio::DeviceInfo tmp;
		tmp.name = listProperty[0];
		std::vector<std::string> listFreq = etk::split(listProperty[2], ',');
		for(size_t fff=0; fff<listFreq.size(); ++fff) {
			tmp.sampleRates.push_back(etk::string_to_int32_t(listFreq[fff]));
		}
		tmp.outputChannels = 0;
		tmp.inputChannels = 0;
		tmp.duplexChannels = 0;
		if (listProperty[1] == "out") {
			tmp.isDefaultOutput = true;
			tmp.isDefaultInput = false;
			tmp.outputChannels = etk::string_to_int32_t(listProperty[3]);
		} else if (listProperty[1] == "in") {
			tmp.isDefaultOutput = false;
			tmp.isDefaultInput = true;
			tmp.inputChannels = etk::string_to_int32_t(listProperty[3]);
		} else {
			/* duplex */
			tmp.isDefaultOutput = true;
			tmp.isDefaultInput = true;
			tmp.duplexChannels = etk::string_to_int32_t(listProperty[3]);
		}
		tmp.nativeFormats = audio::getListFormatFromString(listProperty[4]);
		m_devices.push_back(tmp);
	}
	ATA_INFO("Create Android interface (end)");
}

airtaudio::api::Android::~Android() {
	ATA_INFO("Destroy Android interface");
}

uint32_t airtaudio::api::Android::getDeviceCount() {
	//ATA_INFO("Get device count:"<< m_devices.size());
	return m_devices.size();
}

airtaudio::DeviceInfo airtaudio::api::Android::getDeviceInfo(uint32_t _device) {
	//ATA_INFO("Get device info ...");
	return m_devices[_device];
}

enum airtaudio::error airtaudio::api::Android::closeStream() {
	ATA_INFO("Clese Stream");
	// Can not close the stream now...
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Android::startStream() {
	ATA_INFO("Start Stream");
	// Can not close the stream now...
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Android::stopStream() {
	ATA_INFO("Stop stream");
	ewol::Context& tmpContext = ewol::getContext();
	tmpContext.audioCloseDevice(0);
	// Can not close the stream now...
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Android::abortStream() {
	ATA_INFO("Abort Stream");
	ewol::Context& tmpContext = ewol::getContext();
	tmpContext.audioCloseDevice(0);
	// Can not close the stream now...
	return airtaudio::error_none;
}

void airtaudio::api::Android::callBackEvent(void* _data,
                                            int32_t _frameRate) {
	int32_t doStopStream = 0;
	double streamTime = getStreamTime();
	enum airtaudio::status status = airtaudio::status_ok;
	if (m_stream.doConvertBuffer[airtaudio::mode_output] == true) {
		doStopStream = m_stream.callbackInfo.callback(m_stream.userBuffer[airtaudio::mode_output],
		                                              nullptr,
		                                              _frameRate,
		                                              streamTime,
		                                              status);
		convertBuffer((char*)_data, (char*)m_stream.userBuffer[airtaudio::mode_output], m_stream.convertInfo[airtaudio::mode_output]);
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

void airtaudio::api::Android::androidCallBackEvent(void* _data,
                                                   int32_t _frameRate,
                                                   void* _userData) {
	if (_userData == nullptr) {
		ATA_INFO("callback event ... nullptr pointer");
		return;
	}
	airtaudio::api::Android* myClass = static_cast<airtaudio::api::Android*>(_userData);
	myClass->callBackEvent(_data, _frameRate/2);
}

bool airtaudio::api::Android::probeDeviceOpen(uint32_t _device,
                                              airtaudio::mode _mode,
                                              uint32_t _channels,
                                              uint32_t _firstChannel,
                                              uint32_t _sampleRate,
                                              audio::format _format,
                                              uint32_t *_bufferSize,
                                              airtaudio::StreamOptions *_options) {
	ATA_INFO("Probe : device=" << _device << " channels=" << _channels << " firstChannel=" << _firstChannel << " sampleRate=" << _sampleRate);
	if (_mode != airtaudio::mode_output) {
		ATA_ERROR("Can not start a device input or duplex for Android ...");
		return false;
	}
	m_stream.userFormat = _format;
	m_stream.nUserChannels[modeToIdTable(_mode)] = _channels;
	ewol::Context& tmpContext = ewol::getContext();
	bool ret = false;
	if (_format == SINT8) {
		ret = tmpContext.audioOpenDevice(_device, _sampleRate, _channels, 0, androidCallBackEvent, this);
	} else {
		ret = tmpContext.audioOpenDevice(_device, _sampleRate, _channels, 1, androidCallBackEvent, this);
	}
	m_stream.bufferSize = 256;
	m_stream.sampleRate = _sampleRate;
	m_stream.doByteSwap[modeToIdTable(_mode)] = false; // for endienness ...
	
	// TODO : For now, we write it in hard ==> to bu update later ...
	m_stream.deviceFormat[modeToIdTable(_mode)] = SINT16;
	m_stream.nDeviceChannels[modeToIdTable(_mode)] = 2;
	m_stream.deviceInterleaved[modeToIdTable(_mode)] =	true;
	
	m_stream.doConvertBuffer[modeToIdTable(_mode)] = false;
	if (m_stream.userFormat != m_stream.deviceFormat[modeToIdTable(_mode)]) {
		m_stream.doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (m_stream.nUserChannels[modeToIdTable(_mode)] < m_stream.nDeviceChannels[modeToIdTable(_mode)]) {
		m_stream.doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (    m_stream.deviceInterleaved[modeToIdTable(_mode)] == false
	     && m_stream.nUserChannels[modeToIdTable(_mode)] > 1) {
		m_stream.doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (m_stream.doConvertBuffer[modeToIdTable(_mode)] == true) {
		// Allocate necessary internal buffers.
		uint64_t bufferBytes = m_stream.nUserChannels[modeToIdTable(_mode)] * m_stream.bufferSize * formatBytes(m_stream.userFormat);
		m_stream.userBuffer[modeToIdTable(_mode)] = (char *) calloc(bufferBytes, 1);
		if (m_stream.userBuffer[modeToIdTable(_mode)] == nullptr) {
			ATA_ERROR("airtaudio::api::Android::probeDeviceOpen: error allocating user buffer memory.");
		}
		setConvertInfo(_mode, _firstChannel);
	}
	ATA_INFO("device format : " << m_stream.deviceFormat[modeToIdTable(_mode)] << " user format : " << m_stream.userFormat);
	ATA_INFO("device channels : " << m_stream.nDeviceChannels[modeToIdTable(_mode)] << " user channels : " << m_stream.nUserChannels[modeToIdTable(_mode)]);
	ATA_INFO("do convert buffer : " << m_stream.doConvertBuffer[modeToIdTable(_mode)]);
	if (ret == false) {
		ATA_ERROR("Can not open device.");
	}
	return ret;
}

#endif

