/**
 * @author Edouard DUPIN
 * 
 * @license like MIT (see license file)
 */

#if defined(__ANDROID_JAVA__)

#include <alsa/asoundlib.h>
#include <unistd.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <limits.h>

airtaudio::api::Android(void) {
	// On android, we set a static device ...
	airtaudio::DeviceInfo tmp;
	tmp.name = "speaker";
	tmp.outputChannels = 2;
	tmp.inputChannels = 0;
	tmp.duplexChannels = 1;
	tmp.isDefaultOutput = true;
	tmp.isDefaultInput = false;
	sampleRates.pushBack(44100);
	nativeFormats = SINT16;
	m_devices.push_back(tmp);
	ATA_INFO("Create Android interface");
}

airtaudio::api::~Android(void) {
	ATA_INFO("Destroy Android interface");
}

uint32_t airtaudio::api::getDeviceCount(void) {
	ATA_INFO("Get device count:"<< m_devices.size());
	return m_devices.size();
}

airtaudio::DeviceInfo airtaudio::api::getDeviceInfo(uint32_t _device) {
	ATA_INFO("Get device info ...");
	return m_devices[_device];
}

enum airtaudio::errorType airtaudio::api::closeStream(void) {
	ATA_INFO("Clese Stream");
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::startStream(void) {
	ATA_INFO("Start Stream");
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::stopStream(void) {
	ATA_INFO("Stop stream");
	// Can not close the stream now...
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::abortStream(void) {
	ATA_INFO("Abort Stream");
	// Can not close the stream now...
	return airtaudio::errorNone;
}

void airtaudio::api::callbackEvent(void) {
	ATA_INFO("callback event ...");
}

bool airtaudio::api::probeDeviceOpen(uint32_t _device,
                     airtaudio::api::StreamMode _mode,
                     uint32_t _channels,
                     uint32_t _firstChannel,
                     uint32_t _sampleRate,
                     airtaudio::format _format,
                     uint32_t *_bufferSize,
                     airtaudio::StreamOptions *_options) {
	ATA_INFO("Probe : device=" << _device << " channels=" << _channels << " firstChannel=" << _firstChannel << " sampleRate=" << _sampleRate);
	return true
}

#endif