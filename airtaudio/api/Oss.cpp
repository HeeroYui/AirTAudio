/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */


#if defined(__LINUX_OSS__)
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "soundcard.h"
#include <errno.h>
#include <math.h>

#undef __class__
#define __class__ "api::Oss"

airtaudio::Api* airtaudio::api::Oss::Create() {
	return new airtaudio::api::Oss();
}

static void *ossCallbackHandler(void* _ptr);

// A structure to hold various information related to the OSS API
// implementation.
struct OssHandle {
	int32_t id[2]; // device ids
	bool xrun[2];
	bool triggered;
	std::condition_variable runnable;
	OssHandle():
	  triggered(false) {
		id[0] = 0;
		id[1] = 0;
		xrun[0] = false;
		xrun[1] = false;
	}
};

airtaudio::api::Oss::Oss() {
	// Nothing to do here.
}

airtaudio::api::Oss::~Oss() {
	if (m_stream.state != airtaudio::state_closed) {
		closeStream();
	}
}

uint32_t airtaudio::api::Oss::getDeviceCount() {
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

airtaudio::DeviceInfo airtaudio::api::Oss::getDeviceInfo(uint32_t _device) {
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
		error(airtaudio::error_warning);
		return info;
	}
	// Probe channels
	if (ainfo.caps & PCM_CAP_airtaudio::mode_output) {
		info.outputChannels = ainfo.max_channels;
	}
	if (ainfo.caps & PCM_CAP_airtaudio::mode_input) {
		info.inputChannels = ainfo.max_channels;
	}
	if (ainfo.caps & PCM_CAP_airtaudio::mode_duplex) {
		if (    info.outputChannels > 0
		     && info.inputChannels > 0
		     && ainfo.caps & PCM_CAP_airtaudio::mode_duplex) {
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

bool airtaudio::api::Oss::probeDeviceOpen(uint32_t _device,
                                          StreamMode _mode,
                                          uint32_t _channels,
                                          uint32_t _firstChannel,
                                          uint32_t _sampleRate,
                                          rtaudio::format _format,
                                          uint32_t* _bufferSize,
                                          rtaudio::StreamOptions* _options) {
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
	if (    (    _mode == airtaudio::mode_output
	          && !(ainfo.caps & PCM_CAP_airtaudio::mode_output))
	     || (    _mode == airtaudio::mode_input
	          && !(ainfo.caps & PCM_CAP_airtaudio::mode_input))) {
		if (_mode == airtaudio::mode_output) {
			ATA_ERROR("device (" << ainfo.name << ") does not support output.");
		} else {
			ATA_ERROR("device (" << ainfo.name << ") does not support input.");
		}
		return false;
	}
	int32_t flags = 0;
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (_mode == airtaudio::mode_output) {
		flags |= O_WRONLY;
	} else { // _mode == airtaudio::mode_input
		if (    m_stream.mode == airtaudio::mode_output
		     && m_stream.device[0] == _device) {
			// We just set the same device for playback ... close and reopen for duplex (OSS only).
			close(handle->id[0]);
			handle->id[0] = 0;
			if (!(ainfo.caps & PCM_CAP_airtaudio::mode_duplex)) {
				ATA_ERROR("device (" << ainfo.name << ") does not support duplex mode.");
				return false;
			}
			// Check that the number previously set channels is the same.
			if (m_stream.nUserChannels[0] != _channels) {
				ATA_ERROR("input/output channels must be equal for OSS duplex device (" << ainfo.name << ").");
				return false;
			}
			flags |= O_RDWR;
		} else {
			flags |= O_RDONLY;
		}
	}
	// Set exclusive access if specified.
	if (    _options != nullptr
	     && _options->flags & RTAUDIO_HOG_DEVICE) {
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
			result = ioctl(fd, SNDCTL_DSP_SETairtaudio::mode_duplex, nullptr);
			if (result == -1) {
				m_errorStream << "error setting duplex mode for device (" << ainfo.name << ").";
				m_errorText = m_errorStream.str();
				return false;
			}
		}
	*/
	// Check the device channel support.
	m_stream.nUserChannels[modeToIdTable(_mode)] = _channels;
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
	m_stream.nDeviceChannels[modeToIdTable(_mode)] = deviceChannels;
	// Get the data format mask
	int32_t mask;
	result = ioctl(fd, SNDCTL_DSP_GETFMTS, &mask);
	if (result == -1) {
		close(fd);
		ATA_ERROR("error getting device (" << ainfo.name << ") data formats.");
		return false;
	}
	// Determine how to set the device format.
	m_stream.userFormat = _format;
	int32_t deviceFormat = -1;
	m_stream.doByteSwap[modeToIdTable(_mode)] = false;
	if (_format == RTAUDIO_SINT8) {
		if (mask & AFMT_S8) {
			deviceFormat = AFMT_S8;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT8;
		}
	} else if (_format == RTAUDIO_SINT16) {
		if (mask & AFMT_S16_NE) {
			deviceFormat = AFMT_S16_NE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
		} else if (mask & AFMT_S16_OE) {
			deviceFormat = AFMT_S16_OE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
			m_stream.doByteSwap[modeToIdTable(_mode)] = true;
		}
	} else if (_format == RTAUDIO_SINT24) {
		if (mask & AFMT_S24_NE) {
			deviceFormat = AFMT_S24_NE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
		} else if (mask & AFMT_S24_OE) {
			deviceFormat = AFMT_S24_OE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
			m_stream.doByteSwap[modeToIdTable(_mode)] = true;
		}
	} else if (_format == RTAUDIO_SINT32) {
		if (mask & AFMT_S32_NE) {
			deviceFormat = AFMT_S32_NE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
		} else if (mask & AFMT_S32_OE) {
			deviceFormat = AFMT_S32_OE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
			m_stream.doByteSwap[modeToIdTable(_mode)] = true;
		}
	}
	if (deviceFormat == -1) {
		// The user requested format is not natively supported by the device.
		if (mask & AFMT_S16_NE) {
			deviceFormat = AFMT_S16_NE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
		} else if (mask & AFMT_S32_NE) {
			deviceFormat = AFMT_S32_NE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
		} else if (mask & AFMT_S24_NE) {
			deviceFormat = AFMT_S24_NE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
		} else if (mask & AFMT_S16_OE) {
			deviceFormat = AFMT_S16_OE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT16;
			m_stream.doByteSwap[modeToIdTable(_mode)] = true;
		} else if (mask & AFMT_S32_OE) {
			deviceFormat = AFMT_S32_OE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT32;
			m_stream.doByteSwap[modeToIdTable(_mode)] = true;
		} else if (mask & AFMT_S24_OE) {
			deviceFormat = AFMT_S24_OE;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT24;
			m_stream.doByteSwap[modeToIdTable(_mode)] = true;
		} else if (mask & AFMT_S8) {
			deviceFormat = AFMT_S8;
			m_stream.deviceFormat[modeToIdTable(_mode)] = RTAUDIO_SINT8;
		}
	}
	if (m_stream.deviceFormat[modeToIdTable(_mode)] == 0) {
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
	int32_t ossBufferBytes = *_bufferSize * audio::getFormatBytes(m_stream.deviceFormat[modeToIdTable(_mode)]) * deviceChannels;
	if (ossBufferBytes < 16) {
		ossBufferBytes = 16;
	}
	int32_t buffers = 0;
	if (_options != nullptr) {
		buffers = _options->numberOfBuffers;
	}
	if (    _options != nullptr
	     && _options->flags.m_minimizeLatency == true) {
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
	m_stream.nBuffers = buffers;
	// Save buffer size (in sample frames).
	*_bufferSize = ossBufferBytes / (audio::getFormatBytes(m_stream.deviceFormat[modeToIdTable(_mode)]) * deviceChannels);
	m_stream.bufferSize = *_bufferSize;
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
	m_stream.sampleRate = _sampleRate;
	if (    _mode == airtaudio::mode_input
	     && m_stream._mode == airtaudio::mode_output
	     && m_stream.device[0] == _device) {
		// We're doing duplex setup here.
		m_stream.deviceFormat[0] = m_stream.deviceFormat[1];
		m_stream.nDeviceChannels[0] = deviceChannels;
	}
	// Set interleaving parameters.
	m_stream.deviceInterleaved[modeToIdTable(_mode)] =	true;
	// Set flags for buffer conversion
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
	// Allocate the stream handles if necessary and then save.
	if (m_stream.apiHandle == 0) {
		handle = new OssHandle;
		if handle == nullptr) {
			ATA_ERROR("error allocating OssHandle memory.");
			goto error;
		}
		m_stream.apiHandle = (void *) handle;
	} else {
		handle = (OssHandle *) m_stream.apiHandle;
	}
	handle->id[modeToIdTable(_mode)] = fd;
	// Allocate necessary internal buffers.
	uint64_t bufferBytes;
	bufferBytes = m_stream.nUserChannels[modeToIdTable(_mode)] * *_bufferSize * audio::getFormatBytes(m_stream.userFormat);
	m_stream.userBuffer[modeToIdTable(_mode)] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[modeToIdTable(_mode)] == nullptr) {
		ATA_ERROR("error allocating user buffer memory.");
		goto error;
	}
	if (m_stream.doConvertBuffer[modeToIdTable(_mode)]) {
		bool makeBuffer = true;
		bufferBytes = m_stream.nDeviceChannels[modeToIdTable(_mode)] * audio::getFormatBytes(m_stream.deviceFormat[modeToIdTable(_mode)]);
		if (_mode == airtaudio::mode_input) {
			if (    m_stream._mode == airtaudio::mode_output
			     && m_stream.deviceBuffer) {
				uint64_t bytesOut = m_stream.nDeviceChannels[0] * audio::getFormatBytes(m_stream.deviceFormat[0]);
				if (bufferBytes <= bytesOut) {
					makeBuffer = false;
				}
			}
		}
		if (makeBuffer) {
			bufferBytes *= *_bufferSize;
			if (m_stream.deviceBuffer) {
				free(m_stream.deviceBuffer);
			}
			m_stream.deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_stream.deviceBuffer == nullptr) {
				ATA_ERROR("error allocating device buffer memory.");
				goto error;
			}
		}
	}
	m_stream.device[modeToIdTable(_mode)] = _device;
	m_stream.state = airtaudio::state_stopped;
	// Setup the buffer conversion information structure.
	if (m_stream.doConvertBuffer[modeToIdTable(_mode)]) {
		setConvertInfo(_mode, _firstChannel);
	}
	// Setup thread if necessary.
	if (m_stream.mode == airtaudio::mode_output && _mode == airtaudio::mode_input) {
		// We had already set up an output stream.
		m_stream.mode = airtaudio::mode_duplex;
		if (m_stream.device[0] == _device) {
			handle->id[0] = fd;
		}
	} else {
		m_stream.mode = _mode;
		// Setup callback thread.
		m_stream.callbackInfo.object = (void *) this;
		m_stream.callbackInfo.isRunning = true;
		m_stream.callbackInfo.thread = new std::thread(ossCallbackHandler, &m_stream.callbackInfo);
		if (m_stream.callbackInfo.thread == nullptr) {
			m_stream.callbackInfo.isRunning = false;
			ATA_ERROR("creating callback thread!");
			goto error;
		}
	}
	return true;
error:
	if (handle) {
		if (handle->id[0]) {
			close(handle->id[0]);
		}
		if (handle->id[1]) {
			close(handle->id[1]);
		}
		delete handle;
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

enum airtaudio::error airtaudio::api::Oss::closeStream() {
	if (m_stream.state == airtaudio::state_closed) {
		ATA_ERROR("no open stream to close!");
		return airtaudio::error_warning;
	}
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	m_stream.callbackInfo.isRunning = false;
	m_stream.mutex.lock();
	if (m_stream.state == airtaudio::state_stopped) {
		handle->runnable.notify_one();
	}
	m_stream.mutex.unlock();
	m_stream.callbackInfo.thread->join();
	if (m_stream.state == airtaudio::state_running) {
		if (m_stream.mode == airtaudio::mode_output || m_stream.mode == airtaudio::mode_duplex) {
			ioctl(handle->id[0], SNDCTL_DSP_HALT, 0);
		} else {
			ioctl(handle->id[1], SNDCTL_DSP_HALT, 0);
		}
		m_stream.state = airtaudio::state_stopped;
	}
	if (handle) {
		if (handle->id[0]) {
			close(handle->id[0]);
		}
		if (handle->id[1]) {
			close(handle->id[1]);
		}
		delete handle;
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
	m_stream.mode = airtaudio::mode_unknow;
	m_stream.state = airtaudio::state_closed;
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Oss::startStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_stream.state == airtaudio::state_running) {
		ATA_ERROR("the stream is already running!");
		return airtaudio::error_warning;
	}
	m_stream.mutex.lock();
	m_stream.state = airtaudio::state_running;
	// No need to do anything else here ... OSS automatically starts
	// when fed samples.
	m_stream.mutex.unlock();
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	handle->runnable.notify_one();
}

enum airtaudio::error airtaudio::api::Oss::stopStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_stream.state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return;
	}
	m_stream.mutex.lock();
	// The state might change while waiting on a mutex.
	if (m_stream.state == airtaudio::state_stopped) {
		m_stream.mutex.unlock();
		return;
	}
	int32_t result = 0;
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (    m_stream.mode == airtaudio::mode_output
	     || m_stream.mode == airtaudio::mode_duplex) {
		// Flush the output with zeros a few times.
		char *buffer;
		int32_t samples;
		audio::format format;
		if (m_stream.doConvertBuffer[0]) {
			buffer = m_stream.deviceBuffer;
			samples = m_stream.bufferSize * m_stream.nDeviceChannels[0];
			format = m_stream.deviceFormat[0];
		} else {
			buffer = m_stream.userBuffer[0];
			samples = m_stream.bufferSize * m_stream.nUserChannels[0];
			format = m_stream.userFormat;
		}
		memset(buffer, 0, samples * audio::getFormatBytes(format));
		for (uint32_t i=0; i<m_stream.nBuffers+1; i++) {
			result = write(handle->id[0], buffer, samples * audio::getFormatBytes(format));
			if (result == -1) {
				ATA_ERROR("audio write error.");
				return airtaudio::error_warning;
			}
		}
		result = ioctl(handle->id[0], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping callback procedure on device (" << m_stream.device[0] << ").");
			goto unlock;
		}
		handle->triggered = false;
	}
	if (    m_stream.mode == airtaudio::mode_input
	     || (    m_stream.mode == airtaudio::mode_duplex
	          && handle->id[0] != handle->id[1])) {
		result = ioctl(handle->id[1], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping input callback procedure on device (" << m_stream.device[0] << ").");
			goto unlock;
		}
	}
unlock:
	m_stream.state = airtaudio::state_stopped;
	m_stream.mutex.unlock();
	if (result != -1) {
		return airtaudio::error_none;
	}
	return airtaudio::error_systemError;
}

enum airtaudio::error airtaudio::api::Oss::abortStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_stream.state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return airtaudio::error_warning;
	}
	m_stream.mutex.lock();
	// The state might change while waiting on a mutex.
	if (m_stream.state == airtaudio::state_stopped) {
		m_stream.mutex.unlock();
		return;
	}
	int32_t result = 0;
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (m_stream.mode == airtaudio::mode_output || m_stream.mode == airtaudio::mode_duplex) {
		result = ioctl(handle->id[0], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping callback procedure on device (" << m_stream.device[0] << ").");
			goto unlock;
		}
		handle->triggered = false;
	}
	if (m_stream.mode == airtaudio::mode_input || (m_stream.mode == airtaudio::mode_duplex && handle->id[0] != handle->id[1])) {
		result = ioctl(handle->id[1], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			ATA_ERROR("system error stopping input callback procedure on device (" << m_stream.device[0] << ").");
			goto unlock;
		}
	}
unlock:
	m_stream.state = airtaudio::state_stopped;
	m_stream.mutex.unlock();
	if (result != -1) {
		return airtaudio::error_none;
	}
	return airtaudio::error_systemError;
}

void airtaudio::api::Oss::callbackEvent() {
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (m_stream.state == airtaudio::state_stopped) {
		std::unique_lock<std::mutex> lck(m_stream.mutex);
		handle->runnable.wait(lck);
		if (m_stream.state != airtaudio::state_running) {
			return;
		}
	}
	if (m_stream.state == airtaudio::state_closed) {
		ATA_ERROR("the stream is closed ... this shouldn't happen!");
		return airtaudio::error_warning;
	}
	// Invoke user callback to get fresh output data.
	int32_t doStopStream = 0;
	double streamTime = getStreamTime();
	rtaudio::streamStatus status = 0;
	if (    m_stream.mode != airtaudio::mode_input
	     && handle->xrun[0] == true) {
		status |= RTAUDIO_airtaudio::status_underflow;
		handle->xrun[0] = false;
	}
	if (    m_stream.mode != airtaudio::mode_output
	     && handle->xrun[1] == true) {
		status |= RTAUDIO_airtaudio::mode_input_OVERFLOW;
		handle->xrun[1] = false;
	}
	doStopStream = m_stream.callbackInfo.callback(m_stream.userBuffer[0],
	                                              m_stream.userBuffer[1],
	                                              m_stream.bufferSize,
	                                              streamTime,
	                                              status);
	if (doStopStream == 2) {
		this->abortStream();
		return;
	}
	m_stream.mutex.lock();
	// The state might change while waiting on a mutex.
	if (m_stream.state == airtaudio::state_stopped) {
		goto unlock;
	}
	int32_t result;
	char *buffer;
	int32_t samples;
	audio::format format;
	if (    m_stream.mode == airtaudio::mode_output
	     || m_stream.mode == airtaudio::mode_duplex) {
		// Setup parameters and do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[0]) {
			buffer = m_stream.deviceBuffer;
			convertBuffer(buffer, m_stream.userBuffer[0], m_stream.convertInfo[0]);
			samples = m_stream.bufferSize * m_stream.nDeviceChannels[0];
			format = m_stream.deviceFormat[0];
		} else {
			buffer = m_stream.userBuffer[0];
			samples = m_stream.bufferSize * m_stream.nUserChannels[0];
			format = m_stream.userFormat;
		}
		// Do byte swapping if necessary.
		if (m_stream.doByteSwap[0]) {
			byteSwapBuffer(buffer, samples, format);
		}
		if (    m_stream.mode == airtaudio::mode_duplex
		     && handle->triggered == false) {
			int32_t trig = 0;
			ioctl(handle->id[0], SNDCTL_DSP_SETTRIGGER, &trig);
			result = write(handle->id[0], buffer, samples * audio::getFormatBytes(format));
			trig = PCM_ENABLE_airtaudio::mode_input|PCM_ENABLE_airtaudio::mode_output;
			ioctl(handle->id[0], SNDCTL_DSP_SETTRIGGER, &trig);
			handle->triggered = true;
		} else {
			// Write samples to device.
			result = write(handle->id[0], buffer, samples * audio::getFormatBytes(format));
		}
		if (result == -1) {
			// We'll assume this is an underrun, though there isn't a
			// specific means for determining that.
			handle->xrun[0] = true;
			ATA_ERROR("audio write error.");
			//error(airtaudio::error_warning);
			// Continue on to input section.
		}
	}
	if (    m_stream.mode == airtaudio::mode_input
	     || m_stream.mode == airtaudio::mode_duplex) {
		// Setup parameters.
		if (m_stream.doConvertBuffer[1]) {
			buffer = m_stream.deviceBuffer;
			samples = m_stream.bufferSize * m_stream.nDeviceChannels[1];
			format = m_stream.deviceFormat[1];
		} else {
			buffer = m_stream.userBuffer[1];
			samples = m_stream.bufferSize * m_stream.nUserChannels[1];
			format = m_stream.userFormat;
		}
		// Read samples from device.
		result = read(handle->id[1], buffer, samples * audio::getFormatBytes(format));
		if (result == -1) {
			// We'll assume this is an overrun, though there isn't a
			// specific means for determining that.
			handle->xrun[1] = true;
			ATA_ERROR("audio read error.");
			goto unlock;
		}
		// Do byte swapping if necessary.
		if (m_stream.doByteSwap[1]) {
			byteSwapBuffer(buffer, samples, format);
		}
		// Do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[1]) {
			convertBuffer(m_stream.userBuffer[1], m_stream.deviceBuffer, m_stream.convertInfo[1]);
		}
	}
unlock:
	m_stream.mutex.unlock();
	airtaudio::Api::tickStreamTime();
	if (doStopStream == 1) {
		this->stopStream();
	}
}

static void ossCallbackHandler(void* _ptr) {
	CallbackInfo* info = (CallbackInfo*)_ptr;
	RtApiOss* object = (RtApiOss*)info->object;
	bool *isRunning = &info->isRunning;
	while (*isRunning == true) {
		object->callbackEvent();
	}
}

#endif
