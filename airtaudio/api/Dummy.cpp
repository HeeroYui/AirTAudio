/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#if defined(__AIRTAUDIO_DUMMY__)
#include <airtaudio/api/Dummy.h>
#include <airtaudio/debug.h>

airtaudio::Api* airtaudio::api::Dummy::Create() {
	return new airtaudio::api::Dummy();
}


airtaudio::api::Dummy::Dummy() {
	m_errorText = "airtaudio::api::Dummy: This class provides no functionality.";
	error(airtaudio::errorWarning);
}

uint32_t airtaudio::api::Dummy::getDeviceCount() {
	return 0;
}

rtaudio::DeviceInfo airtaudio::api::Dummy::getDeviceInfo(uint32_t _device) {
	()_device;
	rtaudio::DeviceInfo info;
	return info;
}

enum airtaudio::errorType airtaudio::api::Dummy::closeStream() {
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::Dummy::startStream() {
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::Dummy::stopStream() {
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::Dummy::abortStream() {
	return airtaudio::errorNone;
}

bool airtaudio::api::Dummy::probeDeviceOpen(uint32_t _device,
                                            airtaudio::api::StreamMode _mode,
                                            uint32_t _channels,
                                            uint32_t _firstChannel,
                                            uint32_t _sampleRate,
                                            airtaudio::format _format,
                                            uint32_t *_bufferSize,
                                            airtaudio::StreamOptions *_options) {
	return false;
}

#endif

