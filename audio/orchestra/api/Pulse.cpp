/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */


#if defined(ORCHESTRA_BUILD_PULSE)

extern "C" {
	#include <limits.h>
	#include <stdio.h>
}
#include <audio/orchestra/Interface.hpp>
#include <audio/orchestra/debug.hpp>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <ethread/tools.hpp>
#include <audio/orchestra/api/PulseDeviceList.hpp>
#include <audio/orchestra/api/Pulse.hpp>

ememory::SharedPtr<audio::orchestra::Api> audio::orchestra::api::Pulse::create() {
	return ememory::SharedPtr<audio::orchestra::api::Pulse>(ETK_NEW(audio::orchestra::api::Pulse));
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
					pa_simple* handle;
					ememory::SharedPtr<ethread::Thread> thread;
					bool threadRunning;
					ethread::Semaphore m_semaphore;
					bool runnable;
					PulsePrivate() :
					  handle(0),
					  threadRunning(false),
					  runnable(false) {
						
					}
			};
		}
	}
}
audio::orchestra::api::Pulse::Pulse() :
  m_private(ETK_NEW(audio::orchestra::api::PulsePrivate)) {
	
}

audio::orchestra::api::Pulse::~Pulse() {
	if (m_state != audio::orchestra::state::closed) {
		closeStream();
	}
}

uint32_t audio::orchestra::api::Pulse::getDeviceCount() {
	#if 1
		etk::Vector<audio::orchestra::DeviceInfo> list = audio::orchestra::api::pulse::getDeviceList();
		return list.size();
	#else
		return 1;
	#endif
}

audio::orchestra::DeviceInfo audio::orchestra::api::Pulse::getDeviceInfo(uint32_t _device) {
	etk::Vector<audio::orchestra::DeviceInfo> list = audio::orchestra::api::pulse::getDeviceList();
	if (_device >= list.size()) {
		ATA_ERROR("Request device out of IDs:" << _device << " >= " << list.size());
		return audio::orchestra::DeviceInfo();
	}
	return list[_device];
}



void audio::orchestra::api::Pulse::callbackEvent() {
	ethread::setName("Pulse IO-" + m_name);
	while (m_private->threadRunning == true) {
		callbackEventOneCycle();
	}
}

enum audio::orchestra::error audio::orchestra::api::Pulse::closeStream() {
	m_private->threadRunning = false;
	m_mutex.lock();
	if (m_state == audio::orchestra::state::stopped) {
		m_private->runnable = true;
		m_private->m_semaphore.post();;
	}
	m_mutex.unLock();
	m_private->thread->join();
	if (m_mode == audio::orchestra::mode_output) {
		pa_simple_flush(m_private->handle, null);
	}
	pa_simple_free(m_private->handle);
	m_private->handle = null;
	m_userBuffer[0].clear();
	m_userBuffer[1].clear();
	m_state = audio::orchestra::state::closed;
	m_mode = audio::orchestra::mode_unknow;
	return audio::orchestra::error_none;
}

void audio::orchestra::api::Pulse::callbackEventOneCycle() {
	if (m_state == audio::orchestra::state::stopped) {
		while (!m_private->runnable) {
			m_private->m_semaphore.wait();
		}
		if (m_state != audio::orchestra::state::running) {
			m_mutex.unLock();
			return;
		}
	}
	if (m_state == audio::orchestra::state::closed) {
		ATA_ERROR("the stream is closed ... this shouldn't happen!");
		return;
	}
	audio::Time streamTime = getStreamTime();
	etk::Vector<enum audio::orchestra::status> status;
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
	if (m_state != audio::orchestra::state::running) {
		goto unLock;
	}
	int32_t pa_error;
	size_t bytes;
	if (m_mode == audio::orchestra::mode_output) {
		if (m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)]) {
			convertBuffer(m_deviceBuffer,
			              &m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)][0],
			              m_convertInfo[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)]);
			bytes = m_nDeviceChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)] * m_bufferSize * audio::getFormatBytes(m_deviceFormat[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)]);
		} else {
			bytes = m_nUserChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_output)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		}
		if (pa_simple_write(m_private->handle, pulse_out, bytes, &pa_error) < 0) {
			ATA_ERROR("audio write error, " << pa_strerror(pa_error) << ".");
			return;
		}
	}
	if (m_mode == audio::orchestra::mode_input) {
		if (m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]) {
			bytes = m_nDeviceChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)] * m_bufferSize * audio::getFormatBytes(m_deviceFormat[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]);
		} else {
			bytes = m_nUserChannels[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)] * m_bufferSize * audio::getFormatBytes(m_userFormat);
		}
		if (pa_simple_read(m_private->handle, pulse_in, bytes, &pa_error) < 0) {
			ATA_ERROR("audio read error, " << pa_strerror(pa_error) << ".");
			return;
		}
		if (m_doConvertBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]) {
			convertBuffer(&m_userBuffer[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)][0],
			              m_deviceBuffer,
			              m_convertInfo[audio::orchestra::modeToIdTable(audio::orchestra::mode_input)]);
		}
	}
unLock:
	m_mutex.unLock();
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
	if (m_state == audio::orchestra::state::closed) {
		ATA_ERROR("the stream is not open!");
		return audio::orchestra::error_invalidUse;
	}
	if (m_state == audio::orchestra::state::running) {
		ATA_ERROR("the stream is already running!");
		return audio::orchestra::error_warning;
	}
	m_mutex.lock();
	m_state = audio::orchestra::state::running;
	m_private->runnable = true;
	m_private->m_semaphore.post();
	m_mutex.unLock();
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Pulse::stopStream() {
	if (m_state == audio::orchestra::state::closed) {
		ATA_ERROR("the stream is not open!");
		return audio::orchestra::error_invalidUse;
	}
	if (m_state == audio::orchestra::state::stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_state = audio::orchestra::state::stopped;
	m_mutex.lock();
	if (    m_private != null
	     && m_private->handle != null
	     && m_mode == audio::orchestra::mode_output) {
		int32_t pa_error;
		if (pa_simple_drain(m_private->handle, &pa_error) < 0) {
			ATA_ERROR("error draining output device, " << pa_strerror(pa_error) << ".");
			m_mutex.unLock();
			return audio::orchestra::error_systemError;
		}
	}
	m_state = audio::orchestra::state::stopped;
	m_mutex.unLock();
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Pulse::abortStream() {
	if (m_state == audio::orchestra::state::closed) {
		ATA_ERROR("the stream is not open!");
		return audio::orchestra::error_invalidUse;
	}
	if (m_state == audio::orchestra::state::stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_state = audio::orchestra::state::stopped;
	m_mutex.lock();
	if (    m_private != null
	     && m_private->handle != null
	     && m_mode == audio::orchestra::mode_output) {
		int32_t pa_error;
		if (pa_simple_flush(m_private->handle, &pa_error) < 0) {
			ATA_ERROR("error flushing output device, " << pa_strerror(pa_error) << ".");
			m_mutex.unLock();
			return audio::orchestra::error_systemError;
		}
	}
	m_state = audio::orchestra::state::stopped;
	m_mutex.unLock();
	return audio::orchestra::error_none;
}

bool audio::orchestra::api::Pulse::open(uint32_t _device,
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
			if (m_deviceBuffer == null) {
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
			m_private->handle = pa_simple_new(null, "orchestra", PA_STREAM_RECORD, null, "Record", &ss, null, null, &error);
			if (m_private->handle == null) {
				ATA_ERROR("error connecting input to PulseAudio server.");
				goto error;
			}
			break;
		case audio::orchestra::mode_output:
			m_private->handle = pa_simple_new(null, "orchestra", PA_STREAM_PLAYBACK, null, "Playback", &ss, null, null, &error);
			if (m_private->handle == null) {
				ATA_ERROR("error connecting output to PulseAudio server.");
				goto error;
			}
			break;
		default:
			goto error;
	}
	if (m_mode == audio::orchestra::mode_unknow) {
		m_mode = _mode;
	} else {
		goto error;
	}
	if (m_private->threadRunning == false) {
		m_private->threadRunning = true;
		m_private->thread = ememory::makeShared<ethread::Thread>([&](){callbackEvent();}, "pulseCallback");
		if (m_private->thread == null) {
			ATA_ERROR("error creating thread.");
			goto error;
		}
	}
	m_state = audio::orchestra::state::stopped;
	return true;
error:
	for (int32_t iii=0; iii<2; ++iii) {
		m_userBuffer[iii].clear();
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	return false;
}

#endif
