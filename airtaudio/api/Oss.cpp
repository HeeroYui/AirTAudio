/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */


#if defined(__LINUX_OSS__)
#include <airtaudio/Interface.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "soundcard.h"
#include <errno.h>
#include <math.h>

airtaudio::Api* airtaudio::api::Oss::Create(void) {
	return new airtaudio::api::Oss();
}

static void *ossCallbackHandler(void * ptr);

// A structure to hold various information related to the OSS API
// implementation.
struct OssHandle {
	int32_t id[2];		// device ids
	bool xrun[2];
	bool triggered;
	pthread_cond_t runnable;

	OssHandle(void):
	  triggered(false) {
		id[0] = 0;
		id[1] = 0;
		xrun[0] = false;
		xrun[1] = false;
	}
};

airtaudio::api::Oss::Oss(void) {
	// Nothing to do here.
}

airtaudio::api::Oss::~Oss(void) {
	if (m_stream.state != STREAM_CLOSED) {
		closeStream();
	}
}

uint32_t airtaudio::api::Oss::getDeviceCount(void)
{
	int32_t mixerfd = open("/dev/mixer", O_RDWR, 0);
	if (mixerfd == -1) {
		m_errorText = "airtaudio::api::Oss::getDeviceCount: error opening '/dev/mixer'.";
		error(airtaudio::errorWarning);
		return 0;
	}

	oss_sysinfo sysinfo;
	if (ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo) == -1) {
		close(mixerfd);
		m_errorText = "airtaudio::api::Oss::getDeviceCount: error getting sysinfo, OSS version >= 4.0 is required.";
		error(airtaudio::errorWarning);
		return 0;
	}

	close(mixerfd);
	return sysinfo.numaudios;
}

rtaudio::DeviceInfo airtaudio::api::Oss::getDeviceInfo(uint32_t device)
{
	rtaudio::DeviceInfo info;
	info.probed = false;

	int32_t mixerfd = open("/dev/mixer", O_RDWR, 0);
	if (mixerfd == -1) {
		m_errorText = "airtaudio::api::Oss::getDeviceInfo: error opening '/dev/mixer'.";
		error(airtaudio::errorWarning);
		return info;
	}

	oss_sysinfo sysinfo;
	int32_t result = ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo);
	if (result == -1) {
		close(mixerfd);
		m_errorText = "airtaudio::api::Oss::getDeviceInfo: error getting sysinfo, OSS version >= 4.0 is required.";
		error(airtaudio::errorWarning);
		return info;
	}

	unsigned nDevices = sysinfo.numaudios;
	if (nDevices == 0) {
		close(mixerfd);
		m_errorText = "airtaudio::api::Oss::getDeviceInfo: no devices found!";
		error(airtaudio::errorInvalidUse);
		return info;
	}

	if (device >= nDevices) {
		close(mixerfd);
		m_errorText = "airtaudio::api::Oss::getDeviceInfo: device ID is invalid!";
		error(airtaudio::errorInvalidUse);
		return info;
	}

	oss_audioinfo ainfo;
	ainfo.dev = device;
	result = ioctl(mixerfd, SNDCTL_AUDIOINFO, &ainfo);
	close(mixerfd);
	if (result == -1) {
		m_errorStream << "airtaudio::api::Oss::getDeviceInfo: error getting device (" << ainfo.name << ") info.";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	// Probe channels
	if (ainfo.caps & PCM_CAP_OUTPUT) info.outputChannels = ainfo.max_channels;
	if (ainfo.caps & PCM_CAP_INPUT) info.inputChannels = ainfo.max_channels;
	if (ainfo.caps & PCM_CAP_DUPLEX) {
		if (info.outputChannels > 0 && info.inputChannels > 0 && ainfo.caps & PCM_CAP_DUPLEX)
			info.duplexChannels = (info.outputChannels > info.inputChannels) ? info.inputChannels : info.outputChannels;
	}

	// Probe data formats ... do for input
	uint64_t mask = ainfo.iformats;
	if (mask & AFMT_S16_LE || mask & AFMT_S16_BE)
		info.nativeFormats |= RTAUDIO_SINT16;
	if (mask & AFMT_S8)
		info.nativeFormats |= RTAUDIO_SINT8;
	if (mask & AFMT_S32_LE || mask & AFMT_S32_BE)
		info.nativeFormats |= RTAUDIO_SINT32;
	if (mask & AFMT_FLOAT)
		info.nativeFormats |= RTAUDIO_FLOAT32;
	if (mask & AFMT_S24_LE || mask & AFMT_S24_BE)
		info.nativeFormats |= RTAUDIO_SINT24;

	// Check that we have at least one supported format
	if (info.nativeFormats == 0) {
		m_errorStream << "airtaudio::api::Oss::getDeviceInfo: device (" << ainfo.name << ") data format not supported by RtAudio.";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
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
	}
	else {
		// Check min and max rate values;
		for (uint32_t k=0; k<MAX_SAMPLE_RATES; k++) {
			if (ainfo.min_rate <= (int) SAMPLE_RATES[k] && ainfo.max_rate >= (int) SAMPLE_RATES[k])
				info.sampleRates.push_back(SAMPLE_RATES[k]);
		}
	}

	if (info.sampleRates.size() == 0) {
		m_errorStream << "airtaudio::api::Oss::getDeviceInfo: no supported sample rates found for device (" << ainfo.name << ").";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
	}
	else {
		info.probed = true;
		info.name = ainfo.name;
	}

	return info;
}


bool airtaudio::api::Oss::probeDeviceOpen(uint32_t device, StreamMode mode, uint32_t channels,
																	uint32_t firstChannel, uint32_t sampleRate,
																	rtaudio::format format, uint32_t *bufferSize,
																	rtaudio::StreamOptions *options)
{
	int32_t mixerfd = open("/dev/mixer", O_RDWR, 0);
	if (mixerfd == -1) {
		m_errorText = "airtaudio::api::Oss::probeDeviceOpen: error opening '/dev/mixer'.";
		return FAILURE;
	}

	oss_sysinfo sysinfo;
	int32_t result = ioctl(mixerfd, SNDCTL_SYSINFO, &sysinfo);
	if (result == -1) {
		close(mixerfd);
		m_errorText = "airtaudio::api::Oss::probeDeviceOpen: error getting sysinfo, OSS version >= 4.0 is required.";
		return FAILURE;
	}

	unsigned nDevices = sysinfo.numaudios;
	if (nDevices == 0) {
		// This should not happen because a check is made before this function is called.
		close(mixerfd);
		m_errorText = "airtaudio::api::Oss::probeDeviceOpen: no devices found!";
		return FAILURE;
	}

	if (device >= nDevices) {
		// This should not happen because a check is made before this function is called.
		close(mixerfd);
		m_errorText = "airtaudio::api::Oss::probeDeviceOpen: device ID is invalid!";
		return FAILURE;
	}

	oss_audioinfo ainfo;
	ainfo.dev = device;
	result = ioctl(mixerfd, SNDCTL_AUDIOINFO, &ainfo);
	close(mixerfd);
	if (result == -1) {
		m_errorStream << "airtaudio::api::Oss::getDeviceInfo: error getting device (" << ainfo.name << ") info.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Check if device supports input or output
	if ((mode == OUTPUT && !(ainfo.caps & PCM_CAP_OUTPUT)) ||
			 (mode == INPUT && !(ainfo.caps & PCM_CAP_INPUT))) {
		if (mode == OUTPUT)
			m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: device (" << ainfo.name << ") does not support output.";
		else
			m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: device (" << ainfo.name << ") does not support input.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	int32_t flags = 0;
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (mode == OUTPUT)
		flags |= O_WRONLY;
	else { // mode == INPUT
		if (m_stream.mode == OUTPUT && m_stream.device[0] == device) {
			// We just set the same device for playback ... close and reopen for duplex (OSS only).
			close(handle->id[0]);
			handle->id[0] = 0;
			if (!(ainfo.caps & PCM_CAP_DUPLEX)) {
				m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: device (" << ainfo.name << ") does not support duplex mode.";
				m_errorText = m_errorStream.str();
				return FAILURE;
			}
			// Check that the number previously set channels is the same.
			if (m_stream.nUserChannels[0] != channels) {
				m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: input/output channels must be equal for OSS duplex device (" << ainfo.name << ").";
				m_errorText = m_errorStream.str();
				return FAILURE;
			}
			flags |= O_RDWR;
		}
		else
			flags |= O_RDONLY;
	}

	// Set exclusive access if specified.
	if (options && options->flags & RTAUDIO_HOG_DEVICE) flags |= O_EXCL;

	// Try to open the device.
	int32_t fd;
	fd = open(ainfo.devnode, flags, 0);
	if (fd == -1) {
		if (errno == EBUSY)
			m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: device (" << ainfo.name << ") is busy.";
		else
			m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: error opening device (" << ainfo.name << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// For duplex operation, specifically set this mode (this doesn't seem to work).
	/*
		if (flags | O_RDWR) {
		result = ioctl(fd, SNDCTL_DSP_SETDUPLEX, NULL);
		if (result == -1) {
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: error setting duplex mode for device (" << ainfo.name << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
		}
		}
	*/

	// Check the device channel support.
	m_stream.nUserChannels[mode] = channels;
	if (ainfo.max_channels < (int)(channels + firstChannel)) {
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: the device (" << ainfo.name << ") does not support requested channel parameters.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Set the number of channels.
	int32_t deviceChannels = channels + firstChannel;
	result = ioctl(fd, SNDCTL_DSP_CHANNELS, &deviceChannels);
	if (result == -1 || deviceChannels < (int)(channels + firstChannel)) {
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: error setting channel parameters on device (" << ainfo.name << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	m_stream.nDeviceChannels[mode] = deviceChannels;

	// Get the data format mask
	int32_t mask;
	result = ioctl(fd, SNDCTL_DSP_GETFMTS, &mask);
	if (result == -1) {
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: error getting device (" << ainfo.name << ") data formats.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Determine how to set the device format.
	m_stream.userFormat = format;
	int32_t deviceFormat = -1;
	m_stream.doByteSwap[mode] = false;
	if (format == RTAUDIO_SINT8) {
		if (mask & AFMT_S8) {
			deviceFormat = AFMT_S8;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT8;
		}
	}
	else if (format == RTAUDIO_SINT16) {
		if (mask & AFMT_S16_NE) {
			deviceFormat = AFMT_S16_NE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
		}
		else if (mask & AFMT_S16_OE) {
			deviceFormat = AFMT_S16_OE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
			m_stream.doByteSwap[mode] = true;
		}
	}
	else if (format == RTAUDIO_SINT24) {
		if (mask & AFMT_S24_NE) {
			deviceFormat = AFMT_S24_NE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT24;
		}
		else if (mask & AFMT_S24_OE) {
			deviceFormat = AFMT_S24_OE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT24;
			m_stream.doByteSwap[mode] = true;
		}
	}
	else if (format == RTAUDIO_SINT32) {
		if (mask & AFMT_S32_NE) {
			deviceFormat = AFMT_S32_NE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT32;
		}
		else if (mask & AFMT_S32_OE) {
			deviceFormat = AFMT_S32_OE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT32;
			m_stream.doByteSwap[mode] = true;
		}
	}

	if (deviceFormat == -1) {
		// The user requested format is not natively supported by the device.
		if (mask & AFMT_S16_NE) {
			deviceFormat = AFMT_S16_NE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
		}
		else if (mask & AFMT_S32_NE) {
			deviceFormat = AFMT_S32_NE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT32;
		}
		else if (mask & AFMT_S24_NE) {
			deviceFormat = AFMT_S24_NE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT24;
		}
		else if (mask & AFMT_S16_OE) {
			deviceFormat = AFMT_S16_OE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
			m_stream.doByteSwap[mode] = true;
		}
		else if (mask & AFMT_S32_OE) {
			deviceFormat = AFMT_S32_OE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT32;
			m_stream.doByteSwap[mode] = true;
		}
		else if (mask & AFMT_S24_OE) {
			deviceFormat = AFMT_S24_OE;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT24;
			m_stream.doByteSwap[mode] = true;
		}
		else if (mask & AFMT_S8) {
			deviceFormat = AFMT_S8;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT8;
		}
	}

	if (m_stream.deviceFormat[mode] == 0) {
		// This really shouldn't happen ...
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: device (" << ainfo.name << ") data format not supported by RtAudio.";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Set the data format.
	int32_t temp = deviceFormat;
	result = ioctl(fd, SNDCTL_DSP_SETFMT, &deviceFormat);
	if (result == -1 || deviceFormat != temp) {
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: error setting data format on device (" << ainfo.name << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Attempt to set the buffer size.	According to OSS, the minimum
	// number of buffers is two.	The supposed minimum buffer size is 16
	// bytes, so that will be our lower bound.	The argument to this
	// call is in the form 0xMMMMSSSS (hex), where the buffer size (in
	// bytes) is given as 2^SSSS and the number of buffers as 2^MMMM.
	// We'll check the actual value used near the end of the setup
	// procedure.
	int32_t ossBufferBytes = *bufferSize * formatBytes(m_stream.deviceFormat[mode]) * deviceChannels;
	if (ossBufferBytes < 16) ossBufferBytes = 16;
	int32_t buffers = 0;
	if (options) buffers = options->numberOfBuffers;
	if (options && options->flags & RTAUDIO_MINIMIZE_LATENCY) buffers = 2;
	if (buffers < 2) buffers = 3;
	temp = ((int) buffers << 16) + (int)(log10((double)ossBufferBytes) / log10(2.0));
	result = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &temp);
	if (result == -1) {
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: error setting buffer size on device (" << ainfo.name << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	m_stream.nBuffers = buffers;

	// Save buffer size (in sample frames).
	*bufferSize = ossBufferBytes / (formatBytes(m_stream.deviceFormat[mode]) * deviceChannels);
	m_stream.bufferSize = *bufferSize;

	// Set the sample rate.
	int32_t srate = sampleRate;
	result = ioctl(fd, SNDCTL_DSP_SPEED, &srate);
	if (result == -1) {
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: error setting sample rate (" << sampleRate << ") on device (" << ainfo.name << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}

	// Verify the sample rate setup worked.
	if (abs(srate - sampleRate) > 100) {
		close(fd);
		m_errorStream << "airtaudio::api::Oss::probeDeviceOpen: device (" << ainfo.name << ") does not support sample rate (" << sampleRate << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	m_stream.sampleRate = sampleRate;

	if (mode == INPUT && m_stream.mode == OUTPUT && m_stream.device[0] == device) {
		// We're doing duplex setup here.
		m_stream.deviceFormat[0] = m_stream.deviceFormat[1];
		m_stream.nDeviceChannels[0] = deviceChannels;
	}

	// Set interleaving parameters.
	m_stream.userInterleaved = true;
	m_stream.deviceInterleaved[mode] =	true;
	if (options && options->flags & RTAUDIO_NONINTERLEAVED)
		m_stream.userInterleaved = false;

	// Set flags for buffer conversion
	m_stream.doConvertBuffer[mode] = false;
	if (m_stream.userFormat != m_stream.deviceFormat[mode])
		m_stream.doConvertBuffer[mode] = true;
	if (m_stream.nUserChannels[mode] < m_stream.nDeviceChannels[mode])
		m_stream.doConvertBuffer[mode] = true;
	if (m_stream.userInterleaved != m_stream.deviceInterleaved[mode] &&
			 m_stream.nUserChannels[mode] > 1)
		m_stream.doConvertBuffer[mode] = true;

	// Allocate the stream handles if necessary and then save.
	if (m_stream.apiHandle == 0) {
		try {
			handle = new OssHandle;
		}
		catch (std::bad_alloc&) {
			m_errorText = "airtaudio::api::Oss::probeDeviceOpen: error allocating OssHandle memory.";
			goto error;
		}

		if (pthread_cond_init(&handle->runnable, NULL)) {
			m_errorText = "airtaudio::api::Oss::probeDeviceOpen: error initializing pthread condition variable.";
			goto error;
		}

		m_stream.apiHandle = (void *) handle;
	}
	else {
		handle = (OssHandle *) m_stream.apiHandle;
	}
	handle->id[mode] = fd;

	// Allocate necessary internal buffers.
	uint64_t bufferBytes;
	bufferBytes = m_stream.nUserChannels[mode] * *bufferSize * formatBytes(m_stream.userFormat);
	m_stream.userBuffer[mode] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[mode] == NULL) {
		m_errorText = "airtaudio::api::Oss::probeDeviceOpen: error allocating user buffer memory.";
		goto error;
	}

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
				m_errorText = "airtaudio::api::Oss::probeDeviceOpen: error allocating device buffer memory.";
				goto error;
			}
		}
	}

	m_stream.device[mode] = device;
	m_stream.state = STREAM_STOPPED;

	// Setup the buffer conversion information structure.
	if (m_stream.doConvertBuffer[mode]) setConvertInfo(mode, firstChannel);

	// Setup thread if necessary.
	if (m_stream.mode == OUTPUT && mode == INPUT) {
		// We had already set up an output stream.
		m_stream.mode = DUPLEX;
		if (m_stream.device[0] == device) handle->id[0] = fd;
	}
	else {
		m_stream.mode = mode;

		// Setup callback thread.
		m_stream.callbackInfo.object = (void *) this;

		// Set the thread attributes for joinable and realtime scheduling
		// priority.	The higher priority will only take affect if the
		// program is run as root or suid.
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#ifdef SCHED_RR // Undefined with some OSes (eg: NetBSD 1.6.x with GNU Pthread)
		if (options && options->flags & RTAUDIO_SCHEDULE_REALTIME) {
			struct sched_param param;
			int32_t priority = options->priority;
			int32_t min = sched_get_priority_min(SCHED_RR);
			int32_t max = sched_get_priority_max(SCHED_RR);
			if (priority < min) priority = min;
			else if (priority > max) priority = max;
			param.sched_priority = priority;
			pthread_attr_setschedparam(&attr, &param);
			pthread_attr_setschedpolicy(&attr, SCHED_RR);
		}
		else
			pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
#else
		pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
#endif

		m_stream.callbackInfo.isRunning = true;
		result = pthread_create(&m_stream.callbackInfo.thread, &attr, ossCallbackHandler, &m_stream.callbackInfo);
		pthread_attr_destroy(&attr);
		if (result) {
			m_stream.callbackInfo.isRunning = false;
			m_errorText = "airtaudio::api::Oss::error creating callback thread!";
			goto error;
		}
	}

	return SUCCESS;

 error:
	if (handle) {
		pthread_cond_destroy(&handle->runnable);
		if (handle->id[0]) close(handle->id[0]);
		if (handle->id[1]) close(handle->id[1]);
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

	return FAILURE;
}

void airtaudio::api::Oss::closeStream()
{
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Oss::closeStream(): no open stream to close!";
		error(airtaudio::errorWarning);
		return;
	}

	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	m_stream.callbackInfo.isRunning = false;
	m_stream.mutex.lock();
	if (m_stream.state == STREAM_STOPPED)
		pthread_cond_signal(&handle->runnable);
	m_stream.mutex.unlock();
	pthread_join(m_stream.callbackInfo.thread, NULL);

	if (m_stream.state == STREAM_RUNNING) {
		if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX)
			ioctl(handle->id[0], SNDCTL_DSP_HALT, 0);
		else
			ioctl(handle->id[1], SNDCTL_DSP_HALT, 0);
		m_stream.state = STREAM_STOPPED;
	}

	if (handle) {
		pthread_cond_destroy(&handle->runnable);
		if (handle->id[0]) close(handle->id[0]);
		if (handle->id[1]) close(handle->id[1]);
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

	m_stream.mode = UNINITIALIZED;
	m_stream.state = STREAM_CLOSED;
}

void airtaudio::api::Oss::startStream()
{
	verifyStream();
	if (m_stream.state == STREAM_RUNNING) {
		m_errorText = "airtaudio::api::Oss::startStream(): the stream is already running!";
		error(airtaudio::errorWarning);
		return;
	}

	m_stream.mutex.lock();

	m_stream.state = STREAM_RUNNING;

	// No need to do anything else here ... OSS automatically starts
	// when fed samples.

	m_stream.mutex.unlock();

	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	pthread_cond_signal(&handle->runnable);
}

void airtaudio::api::Oss::stopStream()
{
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Oss::stopStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}

	m_stream.mutex.lock();

	// The state might change while waiting on a mutex.
	if (m_stream.state == STREAM_STOPPED) {
		m_stream.mutex.unlock();
		return;
	}

	int32_t result = 0;
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {

		// Flush the output with zeros a few times.
		char *buffer;
		int32_t samples;
		rtaudio::format format;

		if (m_stream.doConvertBuffer[0]) {
			buffer = m_stream.deviceBuffer;
			samples = m_stream.bufferSize * m_stream.nDeviceChannels[0];
			format = m_stream.deviceFormat[0];
		}
		else {
			buffer = m_stream.userBuffer[0];
			samples = m_stream.bufferSize * m_stream.nUserChannels[0];
			format = m_stream.userFormat;
		}

		memset(buffer, 0, samples * formatBytes(format));
		for (uint32_t i=0; i<m_stream.nBuffers+1; i++) {
			result = write(handle->id[0], buffer, samples * formatBytes(format));
			if (result == -1) {
				m_errorText = "airtaudio::api::Oss::stopStream: audio write error.";
				error(airtaudio::errorWarning);
			}
		}

		result = ioctl(handle->id[0], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			m_errorStream << "airtaudio::api::Oss::stopStream: system error stopping callback procedure on device (" << m_stream.device[0] << ").";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
		handle->triggered = false;
	}

	if (m_stream.mode == INPUT || (m_stream.mode == DUPLEX && handle->id[0] != handle->id[1])) {
		result = ioctl(handle->id[1], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			m_errorStream << "airtaudio::api::Oss::stopStream: system error stopping input callback procedure on device (" << m_stream.device[0] << ").";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}

 unlock:
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.unlock();

	if (result != -1) return;
	error(airtaudio::errorSystemError);
}

void airtaudio::api::Oss::abortStream()
{
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Oss::abortStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}

	m_stream.mutex.lock();

	// The state might change while waiting on a mutex.
	if (m_stream.state == STREAM_STOPPED) {
		m_stream.mutex.unlock();
		return;
	}

	int32_t result = 0;
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {
		result = ioctl(handle->id[0], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			m_errorStream << "airtaudio::api::Oss::abortStream: system error stopping callback procedure on device (" << m_stream.device[0] << ").";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
		handle->triggered = false;
	}

	if (m_stream.mode == INPUT || (m_stream.mode == DUPLEX && handle->id[0] != handle->id[1])) {
		result = ioctl(handle->id[1], SNDCTL_DSP_HALT, 0);
		if (result == -1) {
			m_errorStream << "airtaudio::api::Oss::abortStream: system error stopping input callback procedure on device (" << m_stream.device[0] << ").";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}

 unlock:
	m_stream.state = STREAM_STOPPED;
	m_stream.mutex.unlock();

	if (result != -1) return;
	*error(airtaudio::errorSystemError);
}

void airtaudio::api::Oss::callbackEvent()
{
	OssHandle *handle = (OssHandle *) m_stream.apiHandle;
	if (m_stream.state == STREAM_STOPPED) {
		m_stream.mutex.lock();
		pthread_cond_wait(&handle->runnable, &m_stream.mutex);
		if (m_stream.state != STREAM_RUNNING) {
			m_stream.mutex.unlock();
			return;
		}
		m_stream.mutex.unlock();
	}

	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Oss::callbackEvent(): the stream is closed ... this shouldn't happen!";
		error(airtaudio::errorWarning);
		return;
	}

	// Invoke user callback to get fresh output data.
	int32_t doStopStream = 0;
	airtaudio::AirTAudioCallback callback = (airtaudio::AirTAudioCallback) m_stream.callbackInfo.callback;
	double streamTime = getStreamTime();
	rtaudio::streamStatus status = 0;
	if (m_stream.mode != INPUT && handle->xrun[0] == true) {
		status |= RTAUDIO_OUTPUT_UNDERFLOW;
		handle->xrun[0] = false;
	}
	if (m_stream.mode != OUTPUT && handle->xrun[1] == true) {
		status |= RTAUDIO_INPUT_OVERFLOW;
		handle->xrun[1] = false;
	}
	doStopStream = callback(m_stream.userBuffer[0], m_stream.userBuffer[1],
													 m_stream.bufferSize, streamTime, status, m_stream.callbackInfo.userData);
	if (doStopStream == 2) {
		this->abortStream();
		return;
	}

	m_stream.mutex.lock();

	// The state might change while waiting on a mutex.
	if (m_stream.state == STREAM_STOPPED) goto unlock;

	int32_t result;
	char *buffer;
	int32_t samples;
	rtaudio::format format;

	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {

		// Setup parameters and do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[0]) {
			buffer = m_stream.deviceBuffer;
			convertBuffer(buffer, m_stream.userBuffer[0], m_stream.convertInfo[0]);
			samples = m_stream.bufferSize * m_stream.nDeviceChannels[0];
			format = m_stream.deviceFormat[0];
		}
		else {
			buffer = m_stream.userBuffer[0];
			samples = m_stream.bufferSize * m_stream.nUserChannels[0];
			format = m_stream.userFormat;
		}

		// Do byte swapping if necessary.
		if (m_stream.doByteSwap[0])
			byteSwapBuffer(buffer, samples, format);

		if (m_stream.mode == DUPLEX && handle->triggered == false) {
			int32_t trig = 0;
			ioctl(handle->id[0], SNDCTL_DSP_SETTRIGGER, &trig);
			result = write(handle->id[0], buffer, samples * formatBytes(format));
			trig = PCM_ENABLE_INPUT|PCM_ENABLE_OUTPUT;
			ioctl(handle->id[0], SNDCTL_DSP_SETTRIGGER, &trig);
			handle->triggered = true;
		}
		else
			// Write samples to device.
			result = write(handle->id[0], buffer, samples * formatBytes(format));

		if (result == -1) {
			// We'll assume this is an underrun, though there isn't a
			// specific means for determining that.
			handle->xrun[0] = true;
			m_errorText = "airtaudio::api::Oss::callbackEvent: audio write error.";
			error(airtaudio::errorWarning);
			// Continue on to input section.
		}
	}

	if (m_stream.mode == INPUT || m_stream.mode == DUPLEX) {

		// Setup parameters.
		if (m_stream.doConvertBuffer[1]) {
			buffer = m_stream.deviceBuffer;
			samples = m_stream.bufferSize * m_stream.nDeviceChannels[1];
			format = m_stream.deviceFormat[1];
		}
		else {
			buffer = m_stream.userBuffer[1];
			samples = m_stream.bufferSize * m_stream.nUserChannels[1];
			format = m_stream.userFormat;
		}

		// Read samples from device.
		result = read(handle->id[1], buffer, samples * formatBytes(format));

		if (result == -1) {
			// We'll assume this is an overrun, though there isn't a
			// specific means for determining that.
			handle->xrun[1] = true;
			m_errorText = "airtaudio::api::Oss::callbackEvent: audio read error.";
			error(airtaudio::errorWarning);
			goto unlock;
		}

		// Do byte swapping if necessary.
		if (m_stream.doByteSwap[1])
			byteSwapBuffer(buffer, samples, format);

		// Do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[1])
			convertBuffer(m_stream.userBuffer[1], m_stream.deviceBuffer, m_stream.convertInfo[1]);
	}

 unlock:
	m_stream.mutex.unlock();

	RtApi::tickStreamTime();
	if (doStopStream == 1) this->stopStream();
}

static void *ossCallbackHandler(void *ptr)
{
	CallbackInfo *info = (CallbackInfo *) ptr;
	RtApiOss *object = (RtApiOss *) info->object;
	bool *isRunning = &info->isRunning;

	while (*isRunning == true) {
		pthread_testcancel();
		object->callbackEvent();
	}

	pthread_exit(NULL);
}

//******************** End of __LINUX_OSS__ *********************//
#endif
