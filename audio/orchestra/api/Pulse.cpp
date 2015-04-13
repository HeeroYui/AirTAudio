/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */


#if defined(ORCHESTRA_BUILD_PULSE)

#include <unistd.h>
#include <limits.h>
#include <audio/orchestra/Interface.h>
#include <audio/orchestra/debug.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <cstdio>
#include <etk/thread/tools.h>

#undef __class__
#define __class__ "api::Pulse"

audio::orchestra::Api* audio::orchestra::api::Pulse::Create() {
	return new audio::orchestra::api::Pulse();
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


namespace audio {
	namespace orchestra {
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
}
audio::orchestra::api::Pulse::Pulse() :
  m_private(new audio::orchestra::api::PulsePrivate()) {
	
}

audio::orchestra::api::Pulse::~Pulse() {
	if (m_state != audio::orchestra::state_closed) {
		closeStream();
	}
}

uint32_t audio::orchestra::api::Pulse::getDeviceCount() {
	return 1;
}

audio::orchestra::DeviceInfo audio::orchestra::api::Pulse::getDeviceInfo(uint32_t _device) {
	audio::orchestra::DeviceInfo info;
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
	audio::orchestra::api::Pulse* myClass = reinterpret_cast<audio::orchestra::api::Pulse*>(_userData);
	myClass->callbackEvent();
}

void audio::orchestra::api::Pulse::callbackEvent() {
	etk::thread::setName("Pulse IO-" + m_name);
	while (m_private->threadRunning == true) {
		callbackEventOneCycle();
	}
}

enum audio::orchestra::error audio::orchestra::api::Pulse::closeStream() {
	m_private->threadRunning = false;
	m_mutex.lock();
	if (m_state == audio::orchestra::state_stopped) {
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
	m_state = audio::orchestra::state_closed;
	m_mode = audio::orchestra::mode_unknow;
	return audio::orchestra::error_none;
}

void audio::orchestra::api::Pulse::callbackEventOneCycle() {
	if (m_state == audio::orchestra::state_stopped) {
		std11::unique_lock<std11::mutex> lck(m_mutex);
		while (!m_private->runnable) {
			m_private->runnable_cv.wait(lck);
		}
		if (m_state != audio::orchestra::state_running) {
			m_mutex.unlock();
			return;
		}
	}
	if (m_state == audio::orchestra::state_closed) {
		ATA_ERROR("the stream is closed ... this shouldn't happen!");
		return;
	}
	audio::Time streamTime = getStreamTime();
	std::vector<enum audio::orchestra::status> status;
	int32_t doStopStream = m_callback(&m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)][0],
	                                  streamTime,
	                                  &m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)][0],
	                                  streamTime,
	                                  m_bufferSize,
	                                  status);
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	m_mutex.lock();
	void *pulse_in = m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)] ? m_deviceBuffer : &m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)][0];
	void *pulse_out = m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)] ? m_deviceBuffer : &m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)][0];
	if (m_state != audio::orchestra::state_running) {
		goto unlock;
	}
	int32_t pa_error;
	size_t bytes;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		if (m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)]) {
			convertBuffer(m_deviceBuffer,
			              &m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)][0],
			              m_convertInfo[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)]);
			bytes = m_nDeviceChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)] * m_bufferSize * audio::getFormatBytes(m_deviceFormat[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)]);
		} else {
			bytes = m_nUserChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		}
		if (pa_simple_write(m_private->s_play, pulse_out, bytes, &pa_error) < 0) {
			ATA_ERROR("audio write error, " << pa_strerror(pa_error) << ".");
			return;
		}
	}
	if (m_mode == audio::orchestra::mode_input || m_mode == audio::orchestra::mode_duplex) {
		if (m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]) {
			bytes = m_nDeviceChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)] * m_bufferSize * audio::getFormatBytes(m_deviceFormat[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]);
		} else {
			bytes = m_nUserChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		}
		if (pa_simple_read(m_private->s_rec, pulse_in, bytes, &pa_error) < 0) {
			ATA_ERROR("audio read error, " << pa_strerror(pa_error) << ".");
			return;
		}
		if (m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]) {
			convertBuffer(&m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)][0],
			              m_deviceBuffer,
			              m_convertInfo[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]);
		}
	}
unlock:
	m_mutex.unlock();
	audio::orchestra::Api::tickStreamTime();
	if (doStopStream == 1) {
		stopStream();
		return;
	}
	return;
}

enum audio::orchestra::error audio::orchestra::api::Pulse::startStream() {
	// TODO : Check return ...
	audio::orchestra::Api::startStream();
	if (m_state == audio::orchestra::state_closed) {
		ATA_ERROR("the stream is not open!");
		return audio::orchestra::error_invalidUse;
	}
	if (m_state == audio::orchestra::state_running) {
		ATA_ERROR("the stream is already running!");
		return audio::orchestra::error_warning;
	}
	m_mutex.lock();
	m_state = audio::orchestra::state_running;
	m_private->runnable = true;
	m_private->runnable_cv.notify_one();
	m_mutex.unlock();
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Pulse::stopStream() {
	if (m_state == audio::orchestra::state_closed) {
		ATA_ERROR("the stream is not open!");
		return audio::orchestra::error_invalidUse;
	}
	if (m_state == audio::orchestra::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_state = audio::orchestra::state_stopped;
	m_mutex.lock();
	if (m_private->s_play) {
		int32_t pa_error;
		if (pa_simple_drain(m_private->s_play, &pa_error) < 0) {
			ATA_ERROR("error draining output device, " << pa_strerror(pa_error) << ".");
			m_mutex.unlock();
			return audio::orchestra::error_systemError;
		}
	}
	m_state = audio::orchestra::state_stopped;
	m_mutex.unlock();
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Pulse::abortStream() {
	if (m_state == audio::orchestra::state_closed) {
		ATA_ERROR("the stream is not open!");
		return audio::orchestra::error_invalidUse;
	}
	if (m_state == audio::orchestra::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_state = audio::orchestra::state_stopped;
	m_mutex.lock();
	if (m_private && m_private->s_play) {
		int32_t pa_error;
		if (pa_simple_flush(m_private->s_play, &pa_error) < 0) {
			ATA_ERROR("error flushing output device, " << pa_strerror(pa_error) << ".");
			m_mutex.unlock();
			return audio::orchestra::error_systemError;
		}
	}
	m_state = audio::orchestra::state_stopped;
	m_mutex.unlock();
	return audio::orchestra::error_none;
}

bool audio::orchestra::api::Pulse::probeDeviceOpen(uint32_t _device,
                                            audio::orchestra::mode _mode,
                                            uint32_t _channels,
                                            uint32_t _firstChannel,
                                            uint32_t _sampleRate,
                                            audio::format _format,
                                            uint32_t *_bufferSize,
                                            const audio::orchestra::StreamOptions& _options) {
	uint64_t bufferBytes = 0;
	pa_sample_spec ss;
	if (_device != 0) {
		return false;
	}
	if (_mode != audio::orchestra::mode_input && _mode != audio::orchestra::mode_output) {
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
		if (_mode == audio::orchestra::mode_input) {
			if (m_mode == audio::orchestra::mode_output && m_deviceBuffer) {
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
		case audio::orchestra::mode_input:
			m_private->s_rec = pa_simple_new(nullptr, "orchestra", PA_STREAM_RECORD, nullptr, "Record", &ss, nullptr, nullptr, &error);
			if (!m_private->s_rec) {
				ATA_ERROR("error connecting input to PulseAudio server.");
				goto error;
			}
			break;
		case audio::orchestra::mode_output:
			m_private->s_play = pa_simple_new(nullptr, "orchestra", PA_STREAM_PLAYBACK, nullptr, "Playback", &ss, nullptr, nullptr, &error);
			if (!m_private->s_play) {
				ATA_ERROR("error connecting output to PulseAudio server.");
				goto error;
			}
			break;
		default:
			goto error;
	}
	if (m_mode == audio::orchestra::mode_unknow) {
		m_mode = _mode;
	} else if (m_mode == _mode) {
		goto error;
	}else {
		m_mode = audio::orchestra::mode_duplex;
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
	m_state = audio::orchestra::state_stopped;
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
