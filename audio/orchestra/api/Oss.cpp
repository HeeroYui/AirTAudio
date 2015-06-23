/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */


#if defined(ORCHESTRA_BUILD_OSS)
#include <audio/orchestra/Interface.h>
#include <audio/orchestra/debug.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "soundcard.h"
#include <errno.h>
#include <math.h>

#undef __class__
#define __class__ "api::Oss"

std::shared_ptr<audio::orchestra::Api> audio::orchestra::api::Oss::create() {
	return std::shared_ptr<audio::orchestra::Api>(new audio::orchestra::api::Oss());
}

static void *ossCallbackHandler(void* _userData);


namespace audio {
	namespace orchestra {
		namespace api {
			class OssPrivate {
				public:
					int32_t id[2]; // device ids
					bool xrun[2];
					bool triggered;
					std11::condition_variable runnable;
					std11::shared_ptr<std11::thread> thread;
					bool threadRunning;
					OssPrivate():
					  triggered(false),
					  threadRunning(false) {
						id[0] = 0;
						id[1] = 0;
						xrun[0] = false;
						xrun[1] = false;
					}
			};
		}
	}
}

audio::orchestra::api::Oss::Oss() :
  m_private(new audio::orchestra::api::OssPrivate()) {
	// Nothing to do here.
}

audio::orchestra::api::Oss::~Oss() {
	if (m_state != audio::orchestra::state_closed) {
		closeStream();
	}
}

uint32_t audio::orchestra::api::Oss::getDeviceCount() {
	int32_t mixerfd = open("/dev/mixer", O_RDWR, 0);
	if (mixerfd == -1) {
		ATA_ERROR("error opening '/dev/mixer'.");
		return 0;
	}
	oss_sysinfo sysinfo;
	if (ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo) == -1) {
		close(mixerfd);
		ATA_ERROR("error getting sysinfo, OSS version >= 4.0 is required.");
		return 0;
	}
	close(mixerfd);
	return sysinfo.numaudios;
}

audio::orchestra::DeviceInfo audio::orchestra::api::Oss::getDeviceInfo(uint32_t _device) {
	rtaudio::DeviceInfo info;
	info.probed = false;
	int32_t mixerfd = open("/dev/mixer", O_RDWR, 0);
	if (mixerfd == -1) {
		ATA_ERROR("error opening '/dev/mixer'.");
		return info;
	}
	oss_sysinfo sysinfo;
	int32_t result = ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo);
	if (result == -1) {
		close(mixerfd);
		ATA_ERROR("error getting sysinfo, OSS version >= 4.0 is required.");
		return info;
	}
	unsigned nDevices = sysinfo.numaudios;
	if (nDevices == 0) {
		close(mixerfd);
		ATA_ERROR("no devices found!");
		return info;
	}
	if (_device >= nDevices) {
		close(mixerfd);
		ATA_ERROR("device ID is invalid!");
		return info;
	}
	oss_audioinfo ainfo;
	ainfo.dev = _device;
	result = ioctl(mixerfd, SNDCTL_AUDIOINFO, &ainfo);
	close(mixerfd);
	if (result == -1) {
		ATA_ERROR("error getting device (" << ainfo.name << ") info.");
		error(audio::orchestra::error_warning);
		return info;
	}
	// Probe channels
	if (ainfo.caps & PCM_CAP_audio::orchestra::mode_output) {
		info.outputChannels = ainfo.max_channels;
	}
	if (ainfo.caps & PCM_CAP_audio::orchestra::mode_input) {
		info.inputChannels = ainfo.max_channels;
	}
	if (ainfo.caps & PCM_CAP_audio::orchestra::mode_duplex) {
		if (    info.outputChannels > 0
		     && info.inputChannels > 0
		     && ainfo.caps & PCM_CAP_audio::orchestra::mode_duplex) {
			info.duplexChannels = (info.outputChannels > info.inputChannels) ? info.inputChannels : info.outputChannels;
		}
	}
	// Probe data formats ... do for input
	uint64_t mask = ainfo.iformats;
	if (    mask & AFMT_S16_LE
	     || mask & AFMT_S16_BE) {
		info.nativeFormats.push_back(audio::format_int16);
	}
	if (mask & AFMT_S8) {
		info.nativeFormats.push_back(audio::format_int8);
	}
	if (    mask & AFMT_S32_LE
	     || mask & AFMT_S32_BE) {
		info.nativeFormats.push_back(audio::format_int32);
	}
	if (mask & AFMT_FLOAT) {
		info.nativeFormats.push_back(audio::format_float);
	}
	if (    mask & AFMT_S24_LE
	     || mask & AFMT_S24_BE) {
		info.nativeFormats.push_back(audio::format_int24);
	}
	// Check that we have at least one supported format
	if (info.nativeFormats == 0) {
		ATA_ERROR("device (" << ainfo.name << ") data format not supported by RtAudio.");
		return info;
	}
	// Probe the supported sample rates.
	info.sampleRates.clear();
	if (ainfo.nrates) {
		for (uint32_t i=0; i<ainfo.nrates; i++) {
			for (uint32_t k=0; k<MAX_SAMPLE_RATES; k++) {
				if (ainfo.rates[i] == SAMPLE_RATES[k]) {
					info.sampleRates.push_back(SAMPLE_RATES[k]);
					break;
				}
			}
		}
	} else {
		// Check min and max rate values;
		for (uint32_t k=0; k<MAX_SAMPLE_RATES; k++) {
			if (    ainfo.min_rate <= (int) SAMPLE_RATES[k]
			     && ainfo.max_rate >= (int) SAMPLE_RATES[k]) {
				info.sampleRates.push_back(SAMPLE_RATES[k]);
			}
		}
	}
	if (info.sampleRates.size() == 0) {
		ATA_ERROR("no supported sample rates found for device (" << ainfo.name << ").");
	} else {
		info.probed = true;
		info.name = ainfo.name;
	}
	return info;
}

bool audio::orchestra::api::Oss::probeDeviceOpen(uint32_t _device,
                                          StreamMode _mode,
                                          uint32_t _channels,
                                          uint32_t _firstChannel,
                                          uint32_t _sampleRate,
                                          rtaudio::format _format,
                                          uint32_t* _bufferSize,
                                          const audio::orchestra::StreamOptions& _options) {
	int32_t mixerfd = open("/dev/mixer", O_RDWR, 0);
	if (mixerfd == -1) {
		ATA_ERROR("error opening '/dev/mixer'.");
		return false;
	}
	oss_sysinfo sysinfo;
	int32_t result = ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo);
	if (result == -1) {
		close(mixerfd);
		ATA_ERROR("error getting sysinfo, OSS version >= 4.0 is required.");
		return false;
	}
	unsigned nDevices = sysinfo.numaudios;
	if (nDevices == 0) {
		// This should not happen because a check is made before this function is called.
		close(mixerfd);
		ATA_ERROR("no devices found!");
		return false;
	}
	if (_device >= nDevices) {
		// This should not happen because a check is made before this function is called.
		close(mixerfd);
		ATA_ERROR("device ID is invalid!");
		return false;
	}
	oss_audioinfo ainfo;
	ainfo.dev = _device;
	result = ioctl(mixerfd, SNDCTL_AUDIOINFO, &ainfo);
	close(mixerfd);
	if (result == -1) {
		ATA_ERROR("error getting device (" << ainfo.name << ") info.");
		return false;
	}
	// Check if device supports input or output
	if (    (    _mode == audio::orchestra::mode_output
	          && !(ainfo.caps & PCM_CAP_audio::orchestra::mode_output))
	     || (    _mode == audio::orchestra::mode_input
	          && !(ainfo.caps & PCM_CAP_audio::orchestra::mode_input))) {
		if (_mode == audio::orchestra::mode_output) {
			ATA_ERROR("device (" << ainfo.name << ") does not support output.");
		} else {
			ATA_ERROR("device (" << ainfo.name << ") does not support input.");
		}
		return false;
	}
	int32_t flags = 0;
	if (_mode == audio::orchestra::mode_output) {
		flags |= O_WRONLY;
	} else { // _mode == audio::orchestra::mode_input
		if (    m_mode == audio::orchestra::mode_output
		     && m_device[0] == _device) {
			// We just set the same device for playback ... close and reopen for duplex (OSS only).
			close(m_private->id[0]);
			m_private->id[0] = 0;
			if (!(ainfo.caps & PCM_CAP_audio::orchestra::mode_duplex)) {
				ATA_ERROR("device (" << ainfo.name << ") does not support duplex mode.");
				return false;
			}
			// Check that the number previously set channels is the same.
			if (m_nUserChannels[0] != _channels) {
				ATA_ERROR("input/output channels must be equal for OSS duplex device (" << ainfo.name << ").");
				return false;
			}
			flags |= O_RDWR;
		} else {
			flags |= O_RDONLY;
		}
	}
	// Set exclusive access if specified.
	if (_options.flags & RTAUDIO_HOG_DEVICE) {
		flags |= O_EXCL;
	}
	// Try to open the device.
	int32_t fd;
	fd = open(ainfo.devnode, flags, 0);
	if (fd == -1) {
		if (errno == EBUSY) {
			ATA_ERROR("device (" << ainfo.name << ") is busy.");
		} else {
			ATA_ERROR("error opening device (" << ainfo.name << ").");
		}
		return false;
	}
	// For duplex operation, specifically set this mode (this doesn't seem to work).
	/*
		if (flags | O_RDWR) {
			result = ioctl(fd, SNDCTL_DSP_SETaudio::orchestra::mode_duplex, nullptr);
			if (result == -1) {
				m_errorStream << "error setting duplex mode for device (" << ainfo.name << ").";
				m_errorText = m_errorStream.str();
				return false;
			}
		}
	*/
	// Check the device channel support.
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	if (ainfo.max_channels < (int)(_channels + _firstChannel)) {
		close(fd);
		ATA_ERROR("the device (" << ainfo.name << ") does not support requested channel parameters.");
		return false;
	}
	// Set the number of channels.
	int32_t deviceChannels = _channels + _firstChannel;
	result = ioctl(fd, SNDCTL_DSP_CHANNELS, &deviceChannels);
	if (    result == -1
	     || deviceChannels < (int)(_channels + _firstChannel)) {
		close(fd);
		ATA_ERROR("error setting channel parameters on device (" << ainfo.name << ").");
		return false;
	}
	m_nDeviceChannels[modeToIdTable(_mode)] = deviceChannels;
	// Get the data format mask
	int32_t mask;
	result = ioctl(fd, SNDCTL_DSP_GETFMTS, &mask);
	if (result == -1) {
		close(fd);
		ATA_ERROR("error getting device (" << ainfo.name << ") data formats.");
		return false;
	}
	// Determine how to set the device format.
	m_userFormat = _format;
	int32_t deviceFormat = -1;
	m_doByteSwap[modeToIdTable(_mode)] = false;
	if (_format == RTAUDIO_SINT8) {
		if (mask & AFMT_S8) {
			deviceFormat = AFMT_S8;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT8;
		}
	} else if (_format == RTAUDIO_SINT16) {
		if (mask & AFMT_S16_NE) {
			deviceFormat = AFMT_S16_NE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
		} else if (mask & AFMT_S16_OE) {
			deviceFormat = AFMT_S16_OE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
			m_doByteSwap[modeToIdTable(_mode)] = true;
		}
	} else if (_format == RTAUDIO_SINT24) {
		if (mask & AFMT_S24_NE) {
			deviceFormat = AFMT_S24_NE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
		} else if (mask & AFMT_S24_OE) {
			deviceFormat = AFMT_S24_OE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
			m_doByteSwap[modeToIdTable(_mode)] = true;
		}
	} else if (_format == RTAUDIO_SINT32) {
		if (mask & AFMT_S32_NE) {
			deviceFormat = AFMT_S32_NE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
		} else if (mask & AFMT_S32_OE) {
			deviceFormat = AFMT_S32_OE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
			m_doByteSwap[modeToIdTable(_mode)] = true;
		}
	}
	if (deviceFormat == -1) {
		// The user requested format is not natively supported by the device.
		if (mask & AFMT_S16_NE) {
			deviceFormat = AFMT_S16_NE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
		} else if (mask & AFMT_S32_NE) {
			deviceFormat = AFMT_S32_NE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
		} else if (mask & AFMT_S24_NE) {
			deviceFormat = AFMT_S24_NE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
		} else if (mask & AFMT_S16_OE) {
			deviceFormat = AFMT_S16_OE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
			m_doByteSwap[modeToIdTable(_mode)] = true;
		} else if (mask & AFMT_S32_OE) {
			deviceFormat = AFMT_S32_OE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
			m_doByteSwap[modeToIdTable(_mode)] = true;
		} else if (mask & AFMT_S24_OE) {
			deviceFormat = AFMT_S24_OE;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
			m_doByteSwap[modeToIdTable(_mode)] = true;
		} else if (mask & AFMT_S8) {
			deviceFormat = AFMT_S8;
			m_deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT8;
		}
	}
	if (m_deviceFormat[modeToIdTable(_mode)] == 0) {
		// This really shouldn't happen ...
		close(fd);
		ATA_ERROR("device (" << ainfo.name << ") data format not supported by RtAudio.");
		return false;
	}
	// Set the data format.
	int32_t temp = deviceFormat;
	result = ioctl(fd, SNDCTL_DSP_SETFMT, &deviceFormat);
	if (    result == -1
	     || deviceFormat != temp) {
		close(fd);
		ATA_ERROR("error setting data format on device (" << ainfo.name << ").");
		return false;
	}
	// Attempt to set the buffer size.	According to OSS, the minimum
	// number of buffers is two.	The supposed minimum buffer size is 16
	// bytes, so that will be our lower bound.	The argument to this
	// call is in the form 0xMMMMSSSS (hex), where the buffer size (in
	// bytes) is given as 2^SSSS and the number of buffers as 2^MMMM.
	// We'll check the actual value used near the end of the setup
	// procedure.
	int32_t ossBufferBytes = *_bufferSize * audio::getFormatBytes(m_deviceFormat[modeToIdTable(_mode)]) * deviceChannels;
	if (ossBufferBytes < 16) {
		ossBufferBytes = 16;
	}
	int32_t buffers = 0;
	buffers = _options.numberOfBuffers;
	if (_options.flags.m_minimizeLatency == true) {
		buffers = 2;
	}
	if (buffers < 2) {
		buffers = 3;
	}
	temp = ((int) buffers << 16) + (int)(log10((double)ossBufferBytes) / log10(2.0));
	result = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &temp);
	if (result == -1) {
		close(fd);
		ATA_ERROR("error setting buffer size on device (" << ainfo.name << ").");
		return false;
	}
	m_nBuffers = buffers;
	// Save buffer size (in sample frames).
	*_bufferSize = ossBufferBytes / (audio::getFormatBytes(m_deviceFormat[modeToIdTable(_mode)]) * deviceChannels);
	m_bufferSize = *_bufferSize;
	// Set the sample rate.
	int32_t srate = _sampleRate;
	result = ioctl(fd, SNDCTL_DSP_SPEED, &srate);
	if (result == -1) {
		close(fd);
		ATA_ERROR("error setting sample rate (" << _sampleRate << ") on device (" << ainfo.name << ").");
		return false;
	}
	// Verify the sample rate setup worked.
	if (abs(srate - _sampleRate) > 100) {
		close(fd);
		ATA_ERROR("device (" << ainfo.name << ") does not support sample rate (" << _sampleRate << ").");
		return false;
	}
	m_sampleRate = _sampleRate;
	if (    _mode == audio::orchestra::mode_input
	     && m__mode == audio::orchestra::mode_output
	     && m_device[0] == _device) {
		// We're doing duplex setup here.
		m_deviceFormat[0] = m_deviceFormat[1];
		m_nDeviceChannels[0] = deviceChannels;
	}
	// Set interleaving parameters.
	m_deviceInterleaved[modeToIdTable(_mode)] =	true;
	// Set flags for buffer conversion
	m_doConvertBuffer[modeToIdTable(_mode)] = false;
	if (m_userFormat != m_deviceFormat[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (m_nUserChannels[modeToIdTable(_mode)] < m_nDeviceChannels[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (    m_deviceInterleaved[modeToIdTable(_mode)] == false
	     && m_nUserChannels[modeToIdTable(_mode)] > 1) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	m_private->id[modeToIdTable(_mode)] = fd;
	// Allocate necessary internal buffers.
	uint64_t bufferBytes;
	bufferBytes = m_nUserChannels[modeToIdTable(_mode)] * *_bufferSize * audio::getFormatBytes(m_userFormat);
	m_userBuffer[modeToIdTable(_mode)] = (char *) calloc(bufferBytes, 1);
	if (m_userBuffer[modeToIdTable(_mode)] == nullptr) {
		ATA_ERROR("error allocating user buffer memory.");
		goto error;
	}
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		bool makeBuffer = true;
		bufferBytes = m_nDeviceChannels[modeToIdTable(_mode)] * audio::getFormatBytes(m_deviceFormat[modeToIdTable(_mode)]);
		if (_mode == audio::orchestra::mode_input) {
			if (    m__mode == audio::orchestra::mode_output
			     && m_deviceBuffer) {
				uint64_t bytesOut = m_nDeviceChannels[0] * audio::getFormatBytes(m_deviceFormat[0]);
				if (bufferBytes <= bytesOut) {
					makeBuffer = false;
				}
			}
		}
		if (makeBuffer) {
			bufferBytes *= *_bufferSize;
			if (m_deviceBuffer) {
				free(m_deviceBuffer);
			}
			m_deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_deviceBuffer == nullptr) {
				ATA_ERROR("error allocating device buffer memory.");
				goto error;
			}
		}
	}
	m_device[modeToIdTable(_mode)] = _device;
	m_state = audio::orchestra::state_stopped;
	// Setup the buffer conversion information structure.
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		setConvertInfo(_mode, _firstChannel);
	}
	// Setup thread if necessary.
	if (m_mode == audio::orchestra::mode_output && _mode == audio::orchestra::mode_input) {
		// We had already set up an output stream.
		m_mode = audio::orchestra::mode_duplex;
		if (m_device[0] == _device) {
			m_private->id[0] = fd;
		}
	} else {
		m_mode = _mode;
		// Setup callback thread.
		m_private->threadRunning = true;
		m_private->thread = new std11::thread(ossCallbackHandler, this);
		if (m_private->thread == nullptr) {
			m_private->threadRunning = false;
			ATA_ERROR("creating callback thread!");
			goto error;
		}
	}
	return true;
error:
	if (m_private->id[0] != nullptr) {
		close(m_private->id[0]);
		m_private->id[0] = nullptr;
	}
	if (m_private->id[1] != nullptr) {
		close(m_private->id[1]);
		m_private->id[1] = nullptr;
	}
	for (int32_t i=0; i<2; i++) {
		if (m_userBuffer[i]) {
			free(m_userBuffer[i]);
			m_userBuffer[i] = 0;
		}
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	return false;
}

enum audio::orchestra::error audio::orchestra::api::Oss::closeStream() {
	if (m_state == audio::orchestra::state_closed) {
		ATA_ERROR("no open stream to close!");
		return audio::orchestra::error_warning;
	}
	m_private->threadRunning = false;
	m_mutex.lock();
	if (m_state == audio::orchestra::state_stopped) {
		m_private->runnable.notify_one();
	}
	m_mutex.unlock();
	m_private->thread->join();
	if (m_state == audio::orchestra::state_running) {
		if (m_mode == audio::orchestra::mode_output || m_mode == audio::orchestra::mode_duplex) {
			ioctl(m_private->id[0], SNDCTL_DSP_HALT, 0);
		} else {
			ioctl(m_private->id[1], SNDCTL_DSP_HALT, 0);
		}
		m_state = audio::orchestra::state_stopped;
	}
	if (m_private->id[0] != nullptr) {
		close(m_private->id[0]);
		m_private->id[0] = nullptr;
	}
	if (m_private->id[1] != nullptr) {
		close(m_private->id[1]);
		m_private->id[1] = nullptr;
	}
	for (int32_t i=0; i<2; i++) {
		if (m_userBuffer[i]) {
			free(m_userBuffer[i]);
			m_userBuffer[i] = 0;
		}
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	m_mode = audio::orchestra::mode_unknow;
	m_state = audio::orchestra::state_closed;
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Oss::startStream() {
	// TODO : Check return ...
	audio::orchestra::Api::startStream();
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state_running) {
		ATA_ERROR("the stream is already running!");
		return audio::orchestra::error_warning;
	}
	m_mutex.lock();
	m_state = audio::orchestra::state_running;
	// No need to do anything else here ... OSS automatically starts
	// when fed samples.
	m_mutex.unlock();
	m_private->runnable.notify_one();
}

enum audio::orchestra::error audio::orchestra::api::Oss::stopStream() {
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return;
	}
	m_mutex.lock();
	// The state might change while waiting on a mutex.
	if (m_state == audio::orchestra::state_stopped) {
		m_mutex.unlock();
		return;
	}
	int32_t result = 0;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		// Flush the output with zeros a few times.
		char *buffer;
		int32_t samples;
		audio::format format;
		if (m_doConvertBuffer[0]) {
			buffer = m_deviceBuffer;
			samples = m_bufferSize * m_nDeviceChannels[0];
			format = m_deviceFormat[0];
		} else {
			buffer = m_userBuffer[0];
			samples = m_bufferSize * m_nUserChannels[0];
			format = m_userFormat;
		}
		memset(buffer, 0, samples * audio::getFormatBytes(format));
		for (uint32_t i=0; i<m_nBuffers+1; i++) {
			result = write(m_private->id[0], buffer, samples * audio::getFormatBytes(format));
			if (result == -1) {
				ATA_ERROR("audio write error.");
				return audio::orchestra::error_warning;
			}
		}
		result = ioctl(m_private->id[0], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping callback procedure on device (" << m_device[0] << ").");
			goto unlock;
		}
		m_private->triggered = false;
	}
	if (    m_mode == audio::orchestra::mode_input
	     || (    m_mode == audio::orchestra::mode_duplex
	          && m_private->id[0] != m_private->id[1])) {
		result = ioctl(m_private->id[1], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping input callback procedure on device (" << m_device[0] << ").");
			goto unlock;
		}
	}
unlock:
	m_state = audio::orchestra::state_stopped;
	m_mutex.unlock();
	if (result != -1) {
		return audio::orchestra::error_none;
	}
	return audio::orchestra::error_systemError;
}

enum audio::orchestra::error audio::orchestra::api::Oss::abortStream() {
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_mutex.lock();
	// The state might change while waiting on a mutex.
	if (m_state == audio::orchestra::state_stopped) {
		m_mutex.unlock();
		return;
	}
	int32_t result = 0;
	if (m_mode == audio::orchestra::mode_output || m_mode == audio::orchestra::mode_duplex) {
		result = ioctl(m_private->id[0], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping callback procedure on device (" << m_device[0] << ").");
			goto unlock;
		}
		m_private->triggered = false;
	}
	if (m_mode == audio::orchestra::mode_input || (m_mode == audio::orchestra::mode_duplex && m_private->id[0] != m_private->id[1])) {
		result = ioctl(m_private->id[1], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping input callback procedure on device (" << m_device[0] << ").");
			goto unlock;
		}
	}
unlock:
	m_state = audio::orchestra::state_stopped;
	m_mutex.unlock();
	if (result != -1) {
		return audio::orchestra::error_none;
	}
	return audio::orchestra::error_systemError;
}

void audio::orchestra::api::Oss::callbackEvent() {
	if (m_state == audio::orchestra::state_stopped) {
		std11::unique_lock<std11::mutex> lck(m_mutex);
		m_private->runnable.wait(lck);
		if (m_state != audio::orchestra::state_running) {
			return;
		}
	}
	if (m_state == audio::orchestra::state_closed) {
		ATA_ERROR("the stream is closed ... this shouldn't happen!");
		return audio::orchestra::error_warning;
	}
	// Invoke user callback to get fresh output data.
	int32_t doStopStream = 0;
	audio::Time streamTime = getStreamTime();
	std::vector<enum audio::orchestra::status> status;
	if (    m_mode != audio::orchestra::mode_input
	     && m_private->xrun[0] == true) {
		status.push_back(audio::orchestra::status_underflow);
		m_private->xrun[0] = false;
	}
	if (    m_mode != audio::orchestra::mode_output
	     && m_private->xrun[1] == true) {
		status.push_back(audio::orchestra::status_overflow);
		m_private->xrun[1] = false;
	}
	doStopStream = m_callback(m_userBuffer[1],
	                          streamTime,
	                          m_userBuffer[0],
	                          streamTime,
	                          m_bufferSize,
	                          status);
	if (doStopStream == 2) {
		this->abortStream();
		return;
	}
	m_mutex.lock();
	// The state might change while waiting on a mutex.
	if (m_state == audio::orchestra::state_stopped) {
		goto unlock;
	}
	int32_t result;
	char *buffer;
	int32_t samples;
	audio::format format;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		// Setup parameters and do buffer conversion if necessary.
		if (m_doConvertBuffer[0]) {
			buffer = m_deviceBuffer;
			convertBuffer(buffer, m_userBuffer[0], m_convertInfo[0]);
			samples = m_bufferSize * m_nDeviceChannels[0];
			format = m_deviceFormat[0];
		} else {
			buffer = m_userBuffer[0];
			samples = m_bufferSize * m_nUserChannels[0];
			format = m_userFormat;
		}
		// Do byte swapping if necessary.
		if (m_doByteSwap[0]) {
			byteSwapBuffer(buffer, samples, format);
		}
		if (    m_mode == audio::orchestra::mode_duplex
		     && m_private->triggered == false) {
			int32_t trig = 0;
			ioctl(m_private->id[0], SNDCTL_DSP_SETTRIGGER, &trig);
			result = write(m_private->id[0], buffer, samples * audio::getFormatBytes(format));
			trig = PCM_ENABLE_audio::orchestra::mode_input|PCM_ENABLE_audio::orchestra::mode_output;
			ioctl(m_private->id[0], SNDCTL_DSP_SETTRIGGER, &trig);
			m_private->triggered = true;
		} else {
			// Write samples to device.
			result = write(m_private->id[0], buffer, samples * audio::getFormatBytes(format));
		}
		if (result == -1) {
			// We'll assume this is an underrun, though there isn't a
			// specific means for determining that.
			m_private->xrun[0] = true;
			ATA_ERROR("audio write error.");
			//error(audio::orchestra::error_warning);
			// Continue on to input section.
		}
	}
	if (    m_mode == audio::orchestra::mode_input
	     || m_mode == audio::orchestra::mode_duplex) {
		// Setup parameters.
		if (m_doConvertBuffer[1]) {
			buffer = m_deviceBuffer;
			samples = m_bufferSize * m_nDeviceChannels[1];
			format = m_deviceFormat[1];
		} else {
			buffer = m_userBuffer[1];
			samples = m_bufferSize * m_nUserChannels[1];
			format = m_userFormat;
		}
		// Read samples from device.
		result = read(m_private->id[1], buffer, samples * audio::getFormatBytes(format));
		if (result == -1) {
			// We'll assume this is an overrun, though there isn't a
			// specific means for determining that.
			m_private->xrun[1] = true;
			ATA_ERROR("audio read error.");
			goto unlock;
		}
		// Do byte swapping if necessary.
		if (m_doByteSwap[1]) {
			byteSwapBuffer(buffer, samples, format);
		}
		// Do buffer conversion if necessary.
		if (m_doConvertBuffer[1]) {
			convertBuffer(m_userBuffer[1], m_deviceBuffer, m_convertInfo[1]);
		}
	}
unlock:
	m_mutex.unlock();
	audio::orchestra::Api::tickStreamTime();
	if (doStopStream == 1) {
		this->stopStream();
	}
}

static void ossCallbackHandler(void* _userData) {
	etk::thread::setName("OSS callback-" + m_name);
	audio::orchestra::api::Alsa* myClass = reinterpret_cast<audio::orchestra::api::Oss*>(_userData);
	while (myClass->m_private->threadRunning == true) {
		myClass->callbackEvent();
	}
}

#endif
