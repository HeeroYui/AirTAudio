/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */


#if defined(__LINUX_PULSE__)

#include <unistd.h>
#include <limits.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <cstdio>

#undef __class__
#define __class__ "api::Pulse"

airtaudio::Api* airtaudio::api::Pulse::Create() {
	return new airtaudio::api::Pulse();
}


static const uint32_t SUPPORTED_SAMPLERATES[] = {
	8000,
	16000,
	22050,
	32000,
	44100,
	48000,
	96000,
	0
};

struct rtaudio_pa_format_mapping_t {
	enum audio::format airtaudio_format;
	pa_sample_format_t pa_format;
};

static const rtaudio_pa_format_mapping_t supported_sampleformats[] = {
	{audio::format_int16, PA_SAMPLE_S16LE},
	{audio::format_int32, PA_SAMPLE_S32LE},
	{audio::format_float, PA_SAMPLE_FLOAT32LE},
	{audio::format_unknow, PA_SAMPLE_INVALID}};


namespace airtaudio {
	namespace api {
		class PulsePrivate {
			public:
				pa_simple *s_play;
				pa_simple *s_rec;
				std11::shared_ptr<std11::thread> thread;
				bool threadRunning;
				std11::condition_variable runnable_cv;
				bool runnable;
				PulsePrivate() :
				  s_play(0),
				  s_rec(0),
				  threadRunning(false),
				  runnable(false) {
					
				}
		};
	}
}
airtaudio::api::Pulse::Pulse() :
  m_private(new airtaudio::api::PulsePrivate()) {
	
}

airtaudio::api::Pulse::~Pulse() {
	if (m_state != airtaudio::state_closed) {
		closeStream();
	}
}

uint32_t airtaudio::api::Pulse::getDeviceCount() {
	return 1;
}

airtaudio::DeviceInfo airtaudio::api::Pulse::getDeviceInfo(uint32_t _device) {
	airtaudio::DeviceInfo info;
	info.probed = true;
	info.name = "PulseAudio";
	info.outputChannels = 2;
	info.inputChannels = 2;
	info.duplexChannels = 2;
	info.isDefaultOutput = true;
	info.isDefaultInput = true;
	for (const uint32_t *sr = SUPPORTED_SAMPLERATES; *sr; ++sr) {
		info.sampleRates.push_back(*sr);
	}
	info.nativeFormats.push_back(audio::format_int16);
	info.nativeFormats.push_back(audio::format_int32);
	info.nativeFormats.push_back(audio::format_float);
	return info;
}

static void pulseaudio_callback(void* _userData) {
	airtaudio::api::Pulse* myClass = reinterpret_cast<airtaudio::api::Pulse*>(_userData);
	myClass->callbackEvent();
}

void airtaudio::api::Pulse::callbackEvent() {
	etk::log::setThreadName("Pulse IO-" + m_name);
	while (m_private->threadRunning == true) {
		callbackEventOneCycle();
	}
}

enum airtaudio::error airtaudio::api::Pulse::closeStream() {
	m_private->threadRunning = false;
	m_mutex.lock();
	if (m_state == airtaudio::state_stopped) {
		m_private->runnable = true;
		m_private->runnable_cv.notify_one();;
	}
	m_mutex.unlock();
	m_private->thread->join();
	if (m_private->s_play) {
		pa_simple_flush(m_private->s_play, nullptr);
		pa_simple_free(m_private->s_play);
	}
	if (m_private->s_rec) {
		pa_simple_free(m_private->s_rec);
	}
	m_userBuffer[0].clear();
	m_userBuffer[1].clear();
	m_state = airtaudio::state_closed;
	m_mode = airtaudio::mode_unknow;
	return airtaudio::error_none;
}

void airtaudio::api::Pulse::callbackEventOneCycle() {
	if (m_state == airtaudio::state_stopped) {
		std11::unique_lock<std11::mutex> lck(m_mutex);
		while (!m_private->runnable) {
			m_private->runnable_cv.wait(lck);
		}
		if (m_state != airtaudio::state_running) {
			m_mutex.unlock();
			return;
		}
	}
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("the stream is closed ... this shouldn't happen!");
		return;
	}
	std11::chrono::system_clock::time_point streamTime = getStreamTime();
	std::vector<enum airtaudio::status> status;
	int32_t doStopStream = m_callback(&m_userBuffer[airtaudio::modeToIdTable(airtaudio::mode_input)][0],
	                                  streamTime,
	                                  &m_userBuffer[airtaudio::modeToIdTable(airtaudio::mode_output)][0],
	                                  streamTime,
	                                  m_bufferSize,
	                                  status);
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	m_mutex.lock();
	void *pulse_in = m_doConvertBuffer[airtaudio::modeToIdTable(airtaudio::mode_input)] ? m_deviceBuffer : &m_userBuffer[airtaudio::modeToIdTable(airtaudio::mode_input)][0];
	void *pulse_out = m_doConvertBuffer[airtaudio::modeToIdTable(airtaudio::mode_output)] ? m_deviceBuffer : &m_userBuffer[airtaudio::modeToIdTable(airtaudio::mode_output)][0];
	if (m_state != airtaudio::state_running) {
		goto unlock;
	}
	int32_t pa_error;
	size_t bytes;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		if (m_doConvertBuffer[airtaudio::modeToIdTable(airtaudio::mode_output)]) {
			convertBuffer(m_deviceBuffer,
			              &m_userBuffer[airtaudio::modeToIdTable(airtaudio::mode_output)][0],
			              m_convertInfo[airtaudio::modeToIdTable(airtaudio::mode_output)]);
			bytes = m_nDeviceChannels[airtaudio::modeToIdTable(airtaudio::mode_output)] * m_bufferSize * audio::getFormatBytes(m_deviceFormat[airtaudio::modeToIdTable(airtaudio::mode_output)]);
		} else {
			bytes = m_nUserChannels[airtaudio::modeToIdTable(airtaudio::mode_output)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		}
		if (pa_simple_write(m_private->s_play, pulse_out, bytes, &pa_error) < 0) {
			ATA_ERROR("audio write error, " << pa_strerror(pa_error) << ".");
			return;
		}
	}
	if (m_mode == airtaudio::mode_input || m_mode == airtaudio::mode_duplex) {
		if (m_doConvertBuffer[airtaudio::modeToIdTable(airtaudio::mode_input)]) {
			bytes = m_nDeviceChannels[airtaudio::modeToIdTable(airtaudio::mode_input)] * m_bufferSize * audio::getFormatBytes(m_deviceFormat[airtaudio::modeToIdTable(airtaudio::mode_input)]);
		} else {
			bytes = m_nUserChannels[airtaudio::modeToIdTable(airtaudio::mode_input)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		}
		if (pa_simple_read(m_private->s_rec, pulse_in, bytes, &pa_error) < 0) {
			ATA_ERROR("audio read error, " << pa_strerror(pa_error) << ".");
			return;
		}
		if (m_doConvertBuffer[airtaudio::modeToIdTable(airtaudio::mode_input)]) {
			convertBuffer(&m_userBuffer[airtaudio::modeToIdTable(airtaudio::mode_input)][0],
			              m_deviceBuffer,
			              m_convertInfo[airtaudio::modeToIdTable(airtaudio::mode_input)]);
		}
	}
unlock:
	m_mutex.unlock();
	airtaudio::Api::tickStreamTime();
	if (doStopStream == 1) {
		stopStream();
		return;
	}
	return;
}

enum airtaudio::error airtaudio::api::Pulse::startStream() {
	// TODO : Check return ...
	airtaudio::Api::startStream();
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("the stream is not open!");
		return airtaudio::error_invalidUse;
	}
	if (m_state == airtaudio::state_running) {
		ATA_ERROR("the stream is already running!");
		return airtaudio::error_warning;
	}
	m_mutex.lock();
	m_state = airtaudio::state_running;
	m_private->runnable = true;
	m_private->runnable_cv.notify_one();
	m_mutex.unlock();
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Pulse::stopStream() {
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("the stream is not open!");
		return airtaudio::error_invalidUse;
	}
	if (m_state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return airtaudio::error_warning;
	}
	m_state = airtaudio::state_stopped;
	m_mutex.lock();
	if (m_private->s_play) {
		int32_t pa_error;
		if (pa_simple_drain(m_private->s_play, &pa_error) < 0) {
			ATA_ERROR("error draining output device, " << pa_strerror(pa_error) << ".");
			m_mutex.unlock();
			return airtaudio::error_systemError;
		}
	}
	m_state = airtaudio::state_stopped;
	m_mutex.unlock();
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Pulse::abortStream() {
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("the stream is not open!");
		return airtaudio::error_invalidUse;
	}
	if (m_state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return airtaudio::error_warning;
	}
	m_state = airtaudio::state_stopped;
	m_mutex.lock();
	if (m_private && m_private->s_play) {
		int32_t pa_error;
		if (pa_simple_flush(m_private->s_play, &pa_error) < 0) {
			ATA_ERROR("error flushing output device, " << pa_strerror(pa_error) << ".");
			m_mutex.unlock();
			return airtaudio::error_systemError;
		}
	}
	m_state = airtaudio::state_stopped;
	m_mutex.unlock();
	return airtaudio::error_none;
}

bool airtaudio::api::Pulse::probeDeviceOpen(uint32_t _device,
                                            airtaudio::mode _mode,
                                            uint32_t _channels,
                                            uint32_t _firstChannel,
                                            uint32_t _sampleRate,
                                            audio::format _format,
                                            uint32_t *_bufferSize,
                                            airtaudio::StreamOptions *_options) {
	uint64_t bufferBytes = 0;
	pa_sample_spec ss;
	if (_device != 0) {
		return false;
	}
	if (_mode != airtaudio::mode_input && _mode != airtaudio::mode_output) {
		return false;
	}
	if (_channels != 1 && _channels != 2) {
		ATA_ERROR("unsupported number of channels.");
		return false;
	}
	ss.channels = _channels;
	if (_firstChannel != 0) {
		return false;
	}
	bool sr_found = false;
	for (const uint32_t *sr = SUPPORTED_SAMPLERATES; *sr; ++sr) {
		if (_sampleRate == *sr) {
			sr_found = true;
			m_sampleRate = _sampleRate;
			ss.rate = _sampleRate;
			break;
		}
	}
	if (!sr_found) {
		ATA_ERROR("unsupported sample rate.");
		return false;
	}
	bool sf_found = 0;
	for (const rtaudio_pa_format_mapping_t *sf = supported_sampleformats;
	     sf->airtaudio_format && sf->pa_format != PA_SAMPLE_INVALID;
	     ++sf) {
		if (_format == sf->airtaudio_format) {
			sf_found = true;
			m_userFormat = sf->airtaudio_format;
			ss.format = sf->pa_format;
			break;
		}
	}
	if (!sf_found) {
		ATA_ERROR("unsupported sample format.");
		return false;
	}
	m_deviceInterleaved[modeToIdTable(_mode)] = true;
	m_nBuffers = 1;
	m_doByteSwap[modeToIdTable(_mode)] = false;
	m_doConvertBuffer[modeToIdTable(_mode)] = false;
	m_deviceFormat[modeToIdTable(_mode)] = m_userFormat;
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	m_nDeviceChannels[modeToIdTable(_mode)] = _channels + _firstChannel;
	m_channelOffset[modeToIdTable(_mode)] = 0;
	// Allocate necessary internal buffers.
	bufferBytes = m_nUserChannels[modeToIdTable(_mode)] * *_bufferSize * audio::getFormatBytes(m_userFormat);
	m_userBuffer[modeToIdTable(_mode)].resize(bufferBytes, 0);
	if (m_userBuffer[modeToIdTable(_mode)].size() == 0) {
		ATA_ERROR("error allocating user buffer memory.");
		goto error;
	}
	m_bufferSize = *_bufferSize;
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		bool makeBuffer = true;
		bufferBytes = m_nDeviceChannels[modeToIdTable(_mode)] * audio::getFormatBytes(m_deviceFormat[modeToIdTable(_mode)]);
		if (_mode == airtaudio::mode_input) {
			if (m_mode == airtaudio::mode_output && m_deviceBuffer) {
				uint64_t bytesOut = m_nDeviceChannels[0] * audio::getFormatBytes(m_deviceFormat[0]);
				if (bufferBytes <= bytesOut) makeBuffer = false;
			}
		}
		if (makeBuffer) {
			bufferBytes *= *_bufferSize;
			if (m_deviceBuffer) free(m_deviceBuffer);
			m_deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_deviceBuffer == nullptr) {
				ATA_ERROR("error allocating device buffer memory.");
				goto error;
			}
		}
	}
	m_device[modeToIdTable(_mode)] = _device;
	// Setup the buffer conversion information structure.
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		setConvertInfo(_mode, _firstChannel);
	}
	int32_t error;
	switch (_mode) {
		case airtaudio::mode_input:
			m_private->s_rec = pa_simple_new(nullptr, "airtAudio", PA_STREAM_RECORD, nullptr, "Record", &ss, nullptr, nullptr, &error);
			if (!m_private->s_rec) {
				ATA_ERROR("error connecting input to PulseAudio server.");
				goto error;
			}
			break;
		case airtaudio::mode_output:
			m_private->s_play = pa_simple_new(nullptr, "airtAudio", PA_STREAM_PLAYBACK, nullptr, "Playback", &ss, nullptr, nullptr, &error);
			if (!m_private->s_play) {
				ATA_ERROR("error connecting output to PulseAudio server.");
				goto error;
			}
			break;
		default:
			goto error;
	}
	if (m_mode == airtaudio::mode_unknow) {
		m_mode = _mode;
	} else if (m_mode == _mode) {
		goto error;
	}else {
		m_mode = airtaudio::mode_duplex;
	}
	if (!m_private->threadRunning) {
		m_private->threadRunning = true;
		std11::shared_ptr<std11::thread> tmpThread(new std11::thread(&pulseaudio_callback, this));
		m_private->thread =	std::move(tmpThread);
		if (m_private->thread == nullptr) {
			ATA_ERROR("error creating thread.");
			goto error;
		}
	}
	m_state = airtaudio::state_stopped;
	return true;
error:
	for (int32_t i=0; i<2; i++) {
		m_userBuffer[i].clear();
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	return false;
}

#endif
