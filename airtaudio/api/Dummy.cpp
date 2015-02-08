/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#if defined(__DUMMY__)
#include <airtaudio/api/Dummy.h>
#include <airtaudio/debug.h>

#undef __class__
#define __class__ "api::Dummy"

airtaudio::Api* airtaudio::api::Dummy::Create() {
	return new airtaudio::api::Dummy();
}


airtaudio::api::Dummy::Dummy() {
	ATA_WARNING("This class provides no functionality.");
}

uint32_t airtaudio::api::Dummy::getDeviceCount() {
	return 0;
}

airtaudio::DeviceInfo airtaudio::api::Dummy::getDeviceInfo(uint32_t _device) {
	(void)_device;
	return airtaudio::DeviceInfo();
}

enum airtaudio::error airtaudio::api::Dummy::closeStream() {
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Dummy::startStream() {
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Dummy::stopStream() {
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Dummy::abortStream() {
	return airtaudio::error_none;
}

bool airtaudio::api::Dummy::probeDeviceOpen(uint32_t _device,
                                            airtaudio::mode _mode,
                                            uint32_t _channels,
                                            uint32_t _firstChannel,
                                            uint32_t _sampleRate,
                                            audio::format _format,
                                            uint32_t *_bufferSize,
                                            airtaudio::StreamOptions *_options) {
	return false;
}

#endif

