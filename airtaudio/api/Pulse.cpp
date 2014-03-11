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
// Code written by Peter Meerwald, pmeerw@pmeerw.net
// and Tristan Matthews.

#include <pulse/error.h>
#include <pulse/simple.h>
#include <cstdio>

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
	PulseAudioHandle() : s_play(0), s_rec(0), runnable(false) { }
};

airtaudio::api::Pulse::~Pulse()
{
	if (m_stream.state != STREAM_CLOSED)
		closeStream();
}

uint32_t airtaudio::api::Pulse::getDeviceCount(void) {
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

void airtaudio::api::Pulse::closeStream(void) {
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
			pa_simple_flush(pah->s_play, NULL);
			pa_simple_free(pah->s_play);
		}
		if (pah->s_rec) {
			pa_simple_free(pah->s_rec);
		}
		delete pah;
		m_stream.apiHandle = 0;
	}
	if (m_stream.userBuffer[0] != NULL) {
		free(m_stream.userBuffer[0]);
		m_stream.userBuffer[0] = NULL;
	}
	if (m_stream.userBuffer[1] != NULL) {
		free(m_stream.userBuffer[1]);
		m_stream.userBuffer[1] = NULL;
	}
	m_stream.state = STREAM_CLOSED;
	m_stream.mode = UNINITIALIZED;
}

void airtaudio::api::Pulse::callbackEvent(void) {
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
		m_errorText = "airtaudio::api::Pulse::callbackEvent(): the stream is closed ... "
			"this shouldn't happen!";
		error(airtaudio::errorWarning);
		return;
	}
	airtaudio::AirTAudioCallback callback = (airtaudio::AirTAudioCallback) m_stream.callbackInfo.callback;
	double streamTime = getStreamTime();
	airtaudio::streamStatus status = 0;
	int32_t doStopStream = callback(m_stream.userBuffer[OUTPUT],
	                                m_stream.userBuffer[INPUT],
	                                m_stream.bufferSize,
	                                streamTime,
	                                status,
	                                m_stream.callbackInfo.userData);
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
			m_errorStream << "airtaudio::api::Pulse::callbackEvent: audio write error, " << pa_strerror(pa_error) << ".";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorWarning);
		}
	}
	if (m_stream.mode == INPUT || m_stream.mode == DUPLEX) {
		if (m_stream.doConvertBuffer[INPUT]) {
			bytes = m_stream.nDeviceChannels[INPUT] * m_stream.bufferSize * formatBytes(m_stream.deviceFormat[INPUT]);
		} else {
			bytes = m_stream.nUserChannels[INPUT] * m_stream.bufferSize * formatBytes(m_stream.userFormat);
		}
		if (pa_simple_read(pah->s_rec, pulse_in, bytes, &pa_error) < 0) {
			m_errorStream << "airtaudio::api::Pulse::callbackEvent: audio read error, " << pa_strerror(pa_error) << ".";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorWarning);
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
	}
}

void airtaudio::api::Pulse::startStream(void) {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Pulse::startStream(): the stream is not open!";
		error(airtaudio::errorInvalidUse);
		return;
	}
	if (m_stream.state == STREAM_RUNNING) {
		m_errorText = "airtaudio::api::Pulse::startStream(): the stream is already running!";
		error(airtaudio::errorWarning);
		return;
	}
	m_stream.mutex.lock();
	m_stream.state = STREAM_RUNNING;
	pah->runnable = true;
	pah->runnable_cv.notify_one();
	m_stream.mutex.unlock();
}

void airtaudio::api::Pulse::stopStream(void) {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Pulse::stopStream(): the stream is not open!";
		error(airtaudio::errorInvalidUse);
		return;
	}
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Pulse::stopStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.lock();
	if (pah && pah->s_play) {
		int32_t pa_error;
		if (pa_simple_drain(pah->s_play, &pa_error) < 0) {
			m_errorStream << "airtaudio::api::Pulse::stopStream: error draining output device, " <<
				pa_strerror(pa_error) << ".";
			m_errorText = m_errorStream.str();
			m_stream.mutex.unlock();
			error(airtaudio::errorSystemError);
			return;
		}
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.unlock();
}

void airtaudio::api::Pulse::abortStream(void) {
	PulseAudioHandle *pah = static_cast<PulseAudioHandle*>(m_stream.apiHandle);
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Pulse::abortStream(): the stream is not open!";
		error(airtaudio::errorInvalidUse);
		return;
	}
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Pulse::abortStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.lock();
	if (pah && pah->s_play) {
		int32_t pa_error;
		if (pa_simple_flush(pah->s_play, &pa_error) < 0) {
			m_errorStream << "airtaudio::api::Pulse::abortStream: error flushing output device, " <<
				pa_strerror(pa_error) << ".";
			m_errorText = m_errorStream.str();
			m_stream.mutex.unlock();
			error(airtaudio::errorSystemError);
			return;
		}
	}
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.unlock();
}

bool airtaudio::api::Pulse::probeDeviceOpen(uint32_t device,
                                            airtaudio::api::StreamMode mode,
                                            uint32_t channels,
                                            uint32_t firstChannel,
                                            uint32_t sampleRate,
                                            airtaudio::format format,
                                            uint32_t *bufferSize,
                                            airtaudio::StreamOptions *options) {
	PulseAudioHandle *pah = 0;
	uint64_t bufferBytes = 0;
	pa_sample_spec ss;
	if (device != 0) {
		return false;
	}
	if (mode != INPUT && mode != OUTPUT) {
		return false;
	}
	if (channels != 1 && channels != 2) {
		m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: unsupported number of channels.";
		return false;
	}
	ss.channels = channels;
	if (firstChannel != 0) {
		return false;
	}
	bool sr_found = false;
	for (const uint32_t *sr = SUPPORTED_SAMPLERATES; *sr; ++sr) {
		if (sampleRate == *sr) {
			sr_found = true;
			m_stream.sampleRate = sampleRate;
			ss.rate = sampleRate;
			break;
		}
	}
	if (!sr_found) {
		m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: unsupported sample rate.";
		return false;
	}
	bool sf_found = 0;
	for (const rtaudio_pa_format_mapping_t *sf = supported_sampleformats;
	     sf->airtaudio_format && sf->pa_format != PA_SAMPLE_INVALID;
	     ++sf) {
		if (format == sf->airtaudio_format) {
			sf_found = true;
			m_stream.userFormat = sf->airtaudio_format;
			ss.format = sf->pa_format;
			break;
		}
	}
	if (!sf_found) {
		m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: unsupported sample format.";
		return false;
	}

	// Set interleaving parameters.
	if (options && options->flags & NONINTERLEAVED) {
		m_stream.userInterleaved = false;
	} else {
		m_stream.userInterleaved = true;
	}
	m_stream.deviceInterleaved[mode] = true;
	m_stream.nBuffers = 1;
	m_stream.doByteSwap[mode] = false;
	m_stream.doConvertBuffer[mode] = channels > 1 && !m_stream.userInterleaved;
	m_stream.deviceFormat[mode] = m_stream.userFormat;
	m_stream.nUserChannels[mode] = channels;
	m_stream.nDeviceChannels[mode] = channels + firstChannel;
	m_stream.channelOffset[mode] = 0;

	// Allocate necessary internal buffers.
	bufferBytes = m_stream.nUserChannels[mode] * *bufferSize * formatBytes(m_stream.userFormat);
	m_stream.userBuffer[mode] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[mode] == NULL) {
		m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: error allocating user buffer memory.";
		goto error;
	}
	m_stream.bufferSize = *bufferSize;

	if (m_stream.doConvertBuffer[mode]) {
		bool makeBuffer = true;
		bufferBytes = m_stream.nDeviceChannels[mode] * formatBytes(m_stream.deviceFormat[mode]);
		if (mode == INPUT) {
			if (m_stream.mode == OUTPUT && m_stream.deviceBuffer) {
				uint64_t bytesOut = m_stream.nDeviceChannels[0] * formatBytes(m_stream.deviceFormat[0]);
				if (bufferBytes <= bytesOut) makeBuffer = false;
			}
		}
		if (makeBuffer) {
			bufferBytes *= *bufferSize;
			if (m_stream.deviceBuffer) free(m_stream.deviceBuffer);
			m_stream.deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_stream.deviceBuffer == NULL) {
				m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: error allocating device buffer memory.";
				goto error;
			}
		}
	}
	m_stream.device[mode] = device;
	// Setup the buffer conversion information structure.
	if (m_stream.doConvertBuffer[mode]) {
		setConvertInfo(mode, firstChannel);
	}
	if (!m_stream.apiHandle) {
		PulseAudioHandle *pah = new PulseAudioHandle;
		if (!pah) {
			m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: error allocating memory for handle.";
			goto error;
		}
		m_stream.apiHandle = pah;
	}
	pah = static_cast<PulseAudioHandle *>(m_stream.apiHandle);
	int32_t error;
	switch (mode) {
	case INPUT:
		pah->s_rec = pa_simple_new(NULL, "RtAudio", PA_STREAM_RECORD, NULL, "Record", &ss, NULL, NULL, &error);
		if (!pah->s_rec) {
			m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: error connecting input to PulseAudio server.";
			goto error;
		}
		break;
	case OUTPUT:
		pah->s_play = pa_simple_new(NULL, "RtAudio", PA_STREAM_PLAYBACK, NULL, "Playback", &ss, NULL, NULL, &error);
		if (!pah->s_play) {
			m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: error connecting output to PulseAudio server.";
			goto error;
		}
		break;
	default:
		goto error;
	}
	if (m_stream.mode == UNINITIALIZED) {
		m_stream.mode = mode;
	} else if (m_stream.mode == mode) {
		goto error;
	}else {
		m_stream.mode = DUPLEX;
	}
	if (!m_stream.callbackInfo.isRunning) {
		m_stream.callbackInfo.object = this;
		m_stream.callbackInfo.isRunning = true;
		pah->thread = new std::thread(pulseaudio_callback, (void *)&m_stream.callbackInfo);
		if (pah->thread == NULL) {
			m_errorText = "airtaudio::api::Pulse::probeDeviceOpen: error creating thread.";
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
	return FAILURE;
}

#endif
