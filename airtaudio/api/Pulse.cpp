/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */


#if defined(__LINUX_PULSE__)

#include <unistd.h>
#include <limits.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
// Code written by Peter Meerwald, pmeerw@pmeerw.net
// and Tristan Matthews.

#include <pulse/error.h>
#include <pulse/simple.h>
#include <cstdio>

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
	airtaudio::format airtaudio_format;
	pa_sample_format_t pa_format;
};

static const rtaudio_pa_format_mapping_t supported_sampleformats[] = {
	{airtaudio::SINT16, PA_SAMPLE_S16LE},
	{airtaudio::SINT32, PA_SAMPLE_S32LE},
	{airtaudio::FLOAT32, PA_SAMPLE_FLOAT32LE},
	{0, PA_SAMPLE_INVALID}};

struct PulseAudioHandle {
	pa_simple *s_play;
	pa_simple *s_rec;
	std::thread* thread;
	std::condition_variable runnable_cv;
	bool runnable;
	PulseAudioHandle() :
	  s_play(0),
	  s_rec(0),
	  runnable(false) {
		
	}
};

airtaudio::api::Pulse::~Pulse() {
	if (m_stream.state != STREAM_CLOSED) {
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
	info.nativeFormats = SINT16 | SINT32 | FLOAT32;
	return info;
}

static void pulseaudio_callback(void* _user) {
	airtaudio::CallbackInfo *cbi = static_cast<airtaudio::CallbackInfo *>(_user);
	airtaudio::api::Pulse *context = static_cast<airtaudio::api::Pulse*>(cbi->object);
	volatile bool *isRunning = &cbi->isRunning;
	while (*isRunning) {
		context->callbackEvent();
	}
}

enum airtaudio::errorType airtaudio::api::Pulse::closeStream() {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	m_stream.callbackInfo.isRunning = false;
	if (pah) {
		m_stream.mutex.lock();
		if (m_stream.state == STREAM_STOPPED) {
			pah->runnable = true;
			pah->runnable_cv.notify_one();;
		}
		m_stream.mutex.unlock();
		pah->thread->join();
		if (pah->s_play) {
			pa_simple_flush(pah->s_play, nullptr);
			pa_simple_free(pah->s_play);
		}
		if (pah->s_rec) {
			pa_simple_free(pah->s_rec);
		}
		delete pah;
		m_stream.apiHandle = nullptr;
	}
	if (m_stream.userBuffer[0] != nullptr) {
		free(m_stream.userBuffer[0]);
		m_stream.userBuffer[0] = nullptr;
	}
	if (m_stream.userBuffer[1] != nullptr) {
		free(m_stream.userBuffer[1]);
		m_stream.userBuffer[1] = nullptr;
	}
	m_stream.state = STREAM_CLOSED;
	m_stream.mode = UNINITIALIZED;
	return airtaudio::errorNone;
}

void airtaudio::api::Pulse::callbackEvent() {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	if (m_stream.state == STREAM_STOPPED) {
		std::unique_lock<std::mutex> lck(m_stream.mutex);
		while (!pah->runnable) {
			pah->runnable_cv.wait(lck);
		}
		if (m_stream.state != STREAM_RUNNING) {
			m_stream.mutex.unlock();
			return;
		}
	}
	if (m_stream.state == STREAM_CLOSED) {
		ATA_ERROR("airtaudio::api::Pulse::callbackEvent(): the stream is closed ... this shouldn't happen!");
		return;
	}
	double streamTime = getStreamTime();
	airtaudio::streamStatus status = 0;
	int32_t doStopStream = m_stream.callbackInfo.callback(m_stream.userBuffer[OUTPUT],
	                                                      m_stream.userBuffer[INPUT],
	                                                      m_stream.bufferSize,
	                                                      streamTime,
	                                                      status);
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	m_stream.mutex.lock();
	void *pulse_in = m_stream.doConvertBuffer[INPUT] ? m_stream.deviceBuffer : m_stream.userBuffer[INPUT];
	void *pulse_out = m_stream.doConvertBuffer[OUTPUT] ? m_stream.deviceBuffer : m_stream.userBuffer[OUTPUT];
	if (m_stream.state != STREAM_RUNNING) {
		goto unlock;
	}
	int32_t pa_error;
	size_t bytes;
	if (    m_stream.mode == OUTPUT
	     || m_stream.mode == DUPLEX) {
		if (m_stream.doConvertBuffer[OUTPUT]) {
			convertBuffer(m_stream.deviceBuffer,
			              m_stream.userBuffer[OUTPUT],
			              m_stream.convertInfo[OUTPUT]);
			bytes = m_stream.nDeviceChannels[OUTPUT] * m_stream.bufferSize * formatBytes(m_stream.deviceFormat[OUTPUT]);
		} else {
			bytes = m_stream.nUserChannels[OUTPUT] * m_stream.bufferSize * formatBytes(m_stream.userFormat);
		}
		if (pa_simple_write(pah->s_play, pulse_out, bytes, &pa_error) < 0) {
			ATA_ERROR("airtaudio::api::Pulse::callbackEvent: audio write error, " << pa_strerror(pa_error) << ".");
			return;
		}
	}
	if (m_stream.mode == INPUT || m_stream.mode == DUPLEX) {
		if (m_stream.doConvertBuffer[INPUT]) {
			bytes = m_stream.nDeviceChannels[INPUT] * m_stream.bufferSize * formatBytes(m_stream.deviceFormat[INPUT]);
		} else {
			bytes = m_stream.nUserChannels[INPUT] * m_stream.bufferSize * formatBytes(m_stream.userFormat);
		}
		if (pa_simple_read(pah->s_rec, pulse_in, bytes, &pa_error) < 0) {
			ATA_ERROR("airtaudio::api::Pulse::callbackEvent: audio read error, " << pa_strerror(pa_error) << ".");
			return;
		}
		if (m_stream.doConvertBuffer[INPUT]) {
			convertBuffer(m_stream.userBuffer[INPUT],
			              m_stream.deviceBuffer,
			              m_stream.convertInfo[INPUT]);
		}
	}
unlock:
	m_stream.mutex.unlock();
	airtaudio::Api::tickStreamTime();
	if (doStopStream == 1) {
		stopStream();
		return;
	}
	return;
}

enum airtaudio::errorType airtaudio::api::Pulse::startStream() {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	if (m_stream.state == STREAM_CLOSED) {
		ATA_ERROR("airtaudio::api::Pulse::startStream(): the stream is not open!");
		return airtaudio::errorInvalidUse;
	}
	if (m_stream.state == STREAM_RUNNING) {
		ATA_ERROR("airtaudio::api::Pulse::startStream(): the stream is already running!");
		return airtaudio::errorWarning;
	}
	m_stream.mutex.lock();
	m_stream.state = STREAM_RUNNING;
	pah->runnable = true;
	pah->runnable_cv.notify_one();
	m_stream.mutex.unlock();
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::Pulse::stopStream() {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	if (m_stream.state == STREAM_CLOSED) {
		ATA_ERROR("airtaudio::api::Pulse::stopStream(): the stream is not open!");
		return airtaudio::errorInvalidUse;
	}
	if (m_stream.state == STREAM_STOPPED) {
		ATA_ERROR("airtaudio::api::Pulse::stopStream(): the stream is already stopped!");
		return airtaudio::errorWarning;
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.lock();
	if (pah && pah->s_play) {
		int32_t pa_error;
		if (pa_simple_drain(pah->s_play, &pa_error) < 0) {
			ATA_ERROR("airtaudio::api::Pulse::stopStream: error draining output device, " << pa_strerror(pa_error) << ".");
			m_stream.mutex.unlock();
			return airtaudio::errorSystemError;
		}
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.unlock();
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::Pulse::abortStream() {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle*>(m_stream.apiHandle);
	if (m_stream.state == STREAM_CLOSED) {
		ATA_ERROR("airtaudio::api::Pulse::abortStream(): the stream is not open!");
		return airtaudio::errorInvalidUse;
	}
	if (m_stream.state == STREAM_STOPPED) {
		ATA_ERROR("airtaudio::api::Pulse::abortStream(): the stream is already stopped!");
		return airtaudio::errorWarning;
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.lock();
	if (pah && pah->s_play) {
		int32_t pa_error;
		if (pa_simple_flush(pah->s_play, &pa_error) < 0) {
			ATA_ERROR("airtaudio::api::Pulse::abortStream: error flushing output device, " << pa_strerror(pa_error) << ".");
			m_stream.mutex.unlock();
			return airtaudio::errorSystemError;
		}
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.unlock();
	return airtaudio::errorNone;
}

bool airtaudio::api::Pulse::probeDeviceOpen(uint32_t _device,
                                            airtaudio::api::StreamMode _mode,
                                            uint32_t _channels,
                                            uint32_t _firstChannel,
                                            uint32_t _sampleRate,
                                            airtaudio::format _format,
                                            uint32_t *_bufferSize,
                                            airtaudio::StreamOptions *_options) {
	PulseAudioHandle *pah = 0;
	uint64_t bufferBytes = 0;
	pa_sample_spec ss;
	if (_device != 0) {
		return false;
	}
	if (_mode != INPUT && _mode != OUTPUT) {
		return false;
	}
	if (_channels != 1 && _channels != 2) {
		ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: unsupported number of channels.");
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
			m_stream.sampleRate = _sampleRate;
			ss.rate = _sampleRate;
			break;
		}
	}
	if (!sr_found) {
		ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: unsupported sample rate.");
		return false;
	}
	bool sf_found = 0;
	for (const rtaudio_pa_format_mapping_t *sf = supported_sampleformats;
	     sf->airtaudio_format && sf->pa_format != PA_SAMPLE_INVALID;
	     ++sf) {
		if (_format == sf->airtaudio_format) {
			sf_found = true;
			m_stream.userFormat = sf->airtaudio_format;
			ss.format = sf->pa_format;
			break;
		}
	}
	if (!sf_found) {
		ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: unsupported sample format.");
		return false;
	}
	// Set interleaving parameters.
	if (_options && _options->flags & NONINTERLEAVED) {
		m_stream.userInterleaved = false;
	} else {
		m_stream.userInterleaved = true;
	}
	m_stream.deviceInterleaved[_mode] = true;
	m_stream.nBuffers = 1;
	m_stream.doByteSwap[_mode] = false;
	m_stream.doConvertBuffer[_mode] = _channels > 1 && !m_stream.userInterleaved;
	m_stream.deviceFormat[_mode] = m_stream.userFormat;
	m_stream.nUserChannels[_mode] = _channels;
	m_stream.nDeviceChannels[_mode] = _channels + _firstChannel;
	m_stream.channelOffset[_mode] = 0;
	// Allocate necessary internal buffers.
	bufferBytes = m_stream.nUserChannels[_mode] * *_bufferSize * formatBytes(m_stream.userFormat);
	m_stream.userBuffer[_mode] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[_mode] == nullptr) {
		ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: error allocating user buffer memory.");
		goto error;
	}
	m_stream.bufferSize = *_bufferSize;
	if (m_stream.doConvertBuffer[_mode]) {
		bool makeBuffer = true;
		bufferBytes = m_stream.nDeviceChannels[_mode] * formatBytes(m_stream.deviceFormat[_mode]);
		if (_mode == INPUT) {
			if (m_stream.mode == OUTPUT && m_stream.deviceBuffer) {
				uint64_t bytesOut = m_stream.nDeviceChannels[0] * formatBytes(m_stream.deviceFormat[0]);
				if (bufferBytes <= bytesOut) makeBuffer = false;
			}
		}
		if (makeBuffer) {
			bufferBytes *= *_bufferSize;
			if (m_stream.deviceBuffer) free(m_stream.deviceBuffer);
			m_stream.deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_stream.deviceBuffer == nullptr) {
				ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: error allocating device buffer memory.");
				goto error;
			}
		}
	}
	m_stream.device[_mode] = _device;
	// Setup the buffer conversion information structure.
	if (m_stream.doConvertBuffer[_mode]) {
		setConvertInfo(_mode, _firstChannel);
	}
	if (!m_stream.apiHandle) {
		PulseAudioHandle *pah = new PulseAudioHandle;
		if (!pah) {
			ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: error allocating memory for handle.");
			goto error;
		}
		m_stream.apiHandle = pah;
	}
	pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	int32_t error;
	switch (_mode) {
		case INPUT:
			pah->s_rec = pa_simple_new(nullptr, "airtAudio", PA_STREAM_RECORD, nullptr, "Record", &ss, nullptr, nullptr, &error);
			if (!pah->s_rec) {
				ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: error connecting input to PulseAudio server.");
				goto error;
			}
			break;
		case OUTPUT:
			pah->s_play = pa_simple_new(nullptr, "airtAudio", PA_STREAM_PLAYBACK, nullptr, "Playback", &ss, nullptr, nullptr, &error);
			if (!pah->s_play) {
				ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: error connecting output to PulseAudio server.");
				goto error;
			}
			break;
		default:
			goto error;
	}
	if (m_stream.mode == UNINITIALIZED) {
		m_stream.mode = _mode;
	} else if (m_stream.mode == _mode) {
		goto error;
	}else {
		m_stream.mode = DUPLEX;
	}
	if (!m_stream.callbackInfo.isRunning) {
		m_stream.callbackInfo.object = this;
		m_stream.callbackInfo.isRunning = true;
		pah->thread = new std::thread(pulseaudio_callback, (void *)&m_stream.callbackInfo);
		if (pah->thread == nullptr) {
			ATA_ERROR("airtaudio::api::Pulse::probeDeviceOpen: error creating thread.");
			goto error;
		}
	}
	m_stream.state = STREAM_STOPPED;
	return true;
error:
	if (pah && m_stream.callbackInfo.isRunning) {
		delete pah;
		m_stream.apiHandle = 0;
	}
	for (int32_t i=0; i<2; i++) {
		if (m_stream.userBuffer[i]) {
			free(m_stream.userBuffer[i]);
			m_stream.userBuffer[i] = 0;
		}
	}
	if (m_stream.deviceBuffer) {
		free(m_stream.deviceBuffer);
		m_stream.deviceBuffer = 0;
	}
	return false;
}

#endif
