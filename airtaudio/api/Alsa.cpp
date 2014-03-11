/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */


#if defined(__LINUX_ALSA__)

#include <alsa/asoundlib.h>
#include <unistd.h>
#include <airtaudio/Interface.h>
#include <limits.h>

airtaudio::Api* airtaudio::api::Alsa::Create(void) {
	return new airtaudio::api::Alsa();
}


// A structure to hold various information related to the ALSA API
// implementation.
struct AlsaHandle {
	snd_pcm_t *handles[2];
	bool synchronized;
	bool xrun[2];
	std::condition_variable runnable_cv;
	bool runnable;

	AlsaHandle(void) :
	  synchronized(false),
	  runnable(false) {
		xrun[0] = false;
		xrun[1] = false;
	}
};

static void alsaCallbackHandler(void * _ptr);

airtaudio::api::Alsa::Alsa(void) {
	// Nothing to do here.
}

airtaudio::api::Alsa::~Alsa(void) {
	if (m_stream.state != STREAM_CLOSED) {
		closeStream();
	}
}

uint32_t airtaudio::api::Alsa::getDeviceCount(void) {
	unsigned nDevices = 0;
	int32_t result, subdevice, card;
	char name[64];
	snd_ctl_t *handle;
	// Count cards and devices
	card = -1;
	snd_card_next(&card);
	while (card >= 0) {
		sprintf(name, "hw:%d", card);
		result = snd_ctl_open(&handle, name, 0);
		if (result < 0) {
			m_errorStream << "airtaudio::api::Alsa::getDeviceCount: control open, card = " << card << ", " << snd_strerror(result) << ".";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorWarning);
			goto nextcard;
		}
		subdevice = -1;
		while(1) {
			result = snd_ctl_pcm_next_device(handle, &subdevice);
			if (result < 0) {
				m_errorStream << "airtaudio::api::Alsa::getDeviceCount: control next device, card = " << card << ", " << snd_strerror(result) << ".";
				m_errorText = m_errorStream.str();
				error(airtaudio::errorWarning);
				break;
			}
			if (subdevice < 0) {
				break;
			}
			nDevices++;
		}

nextcard:
		snd_ctl_close(handle);
		snd_card_next(&card);
	}
	result = snd_ctl_open(&handle, "default", 0);
	if (result == 0) {
		nDevices++;
		snd_ctl_close(handle);
	}
	return nDevices;
}

airtaudio::DeviceInfo airtaudio::api::Alsa::getDeviceInfo(uint32_t _device) {
	airtaudio::DeviceInfo info;
	info.probed = false;
	unsigned nDevices = 0;
	int32_t result, subdevice, card;
	char name[64];
	snd_ctl_t *chandle;
	// Count cards and devices
	card = -1;
	snd_card_next(&card);
	while (card >= 0) {
		sprintf(name, "hw:%d", card);
		result = snd_ctl_open(&chandle, name, SND_CTL_NONBLOCK);
		if (result < 0) {
			m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: control open, card = " << card << ", " << snd_strerror(result) << ".";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorWarning);
			goto nextcard;
		}
		subdevice = -1;
		while(1) {
			result = snd_ctl_pcm_next_device(chandle, &subdevice);
			if (result < 0) {
				m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: control next device, card = " << card << ", " << snd_strerror(result) << ".";
				m_errorText = m_errorStream.str();
				error(airtaudio::errorWarning);
				break;
			}
			if (subdevice < 0) {
				break;
			}
			if (nDevices == _device) {
				sprintf(name, "hw:%d,%d", card, subdevice);
				goto foundDevice;
			}
			nDevices++;
		}
	nextcard:
		snd_ctl_close(chandle);
		snd_card_next(&card);
	}
	result = snd_ctl_open(&chandle, "default", SND_CTL_NONBLOCK);
	if (result == 0) {
		if (nDevices == _device) {
			strcpy(name, "default");
			goto foundDevice;
		}
		nDevices++;
	}
	if (nDevices == 0) {
		m_errorText = "airtaudio::api::Alsa::getDeviceInfo: no devices found!";
		error(airtaudio::errorInvalidUse);
		return info;
	}
	if (_device >= nDevices) {
		m_errorText = "airtaudio::api::Alsa::getDeviceInfo: device ID is invalid!";
		error(airtaudio::errorInvalidUse);
		return info;
	}

foundDevice:
	// If a stream is already open, we cannot probe the stream devices.
	// Thus, use the saved results.
	if (    m_stream.state != STREAM_CLOSED
	     && (    m_stream.device[0] == _device
	          || m_stream.device[1] == _device)) {
		snd_ctl_close(chandle);
		if (_device >= m_devices.size()) {
			m_errorText = "airtaudio::api::Alsa::getDeviceInfo: device ID was not present before stream was opened.";
			error(airtaudio::errorWarning);
			return info;
		}
		return m_devices[ _device ];
	}
	int32_t openMode = SND_PCM_ASYNC;
	snd_pcm_stream_t stream;
	snd_pcm_info_t *pcminfo;
	snd_pcm_info_alloca(&pcminfo);
	snd_pcm_t *phandle;
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	// First try for playback unless default _device (which has subdev -1)
	stream = SND_PCM_STREAM_PLAYBACK;
	snd_pcm_info_set_stream(pcminfo, stream);
	if (subdevice != -1) {
		snd_pcm_info_set_device(pcminfo, subdevice);
		snd_pcm_info_set_subdevice(pcminfo, 0);
		result = snd_ctl_pcm_info(chandle, pcminfo);
		if (result < 0) {
			// Device probably doesn't support playback.
			goto captureProbe;
		}
	}
	result = snd_pcm_open(&phandle, name, stream, openMode | SND_PCM_NONBLOCK);
	if (result < 0) {
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: snd_pcm_open error for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		goto captureProbe;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: snd_pcm_hw_params error for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		goto captureProbe;
	}
	// Get output channel information.
	uint32_t value;
	result = snd_pcm_hw_params_get_channels_max(params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: error getting device (" << name << ") output channels, " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		goto captureProbe;
	}
	info.outputChannels = value;
	snd_pcm_close(phandle);

captureProbe:
	stream = SND_PCM_STREAM_CAPTURE;
	snd_pcm_info_set_stream(pcminfo, stream);
	// Now try for capture unless default device (with subdev = -1)
	if (subdevice != -1) {
		result = snd_ctl_pcm_info(chandle, pcminfo);
		snd_ctl_close(chandle);
		if (result < 0) {
			// Device probably doesn't support capture.
			if (info.outputChannels == 0) {
				return info;
			}
			goto probeParameters;
		}
	}
	result = snd_pcm_open(&phandle, name, stream, openMode | SND_PCM_NONBLOCK);
	if (result < 0) {
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: snd_pcm_open error for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		if (info.outputChannels == 0) {
			return info;
		}
		goto probeParameters;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: snd_pcm_hw_params error for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		if (info.outputChannels == 0) {
			return info;
		}
		goto probeParameters;
	}
	result = snd_pcm_hw_params_get_channels_max(params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: error getting device (" << name << ") input channels, " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		if (info.outputChannels == 0) {
			return info;
		}
		goto probeParameters;
	}
	info.inputChannels = value;
	snd_pcm_close(phandle);
	// If device opens for both playback and capture, we determine the channels.
	if (info.outputChannels > 0 && info.inputChannels > 0) {
		info.duplexChannels = (info.outputChannels > info.inputChannels) ? info.inputChannels : info.outputChannels;
	}
	// ALSA doesn't provide default devices so we'll use the first available one.
	if (_device == 0 && info.outputChannels > 0) {
		info.isDefaultOutput = true;
	}
	if (_device == 0 && info.inputChannels > 0) {
		info.isDefaultInput = true;
	}

probeParameters:
	// At this point, we just need to figure out the supported data
	// formats and sample rates.	We'll proceed by opening the device in
	// the direction with the maximum number of channels, or playback if
	// they are equal.	This might limit our sample rate options, but so
	// be it.
	if (info.outputChannels >= info.inputChannels) {
		stream = SND_PCM_STREAM_PLAYBACK;
	} else {
		stream = SND_PCM_STREAM_CAPTURE;
	}
	snd_pcm_info_set_stream(pcminfo, stream);
	result = snd_pcm_open(&phandle, name, stream, openMode | SND_PCM_NONBLOCK);
	if (result < 0) {
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: snd_pcm_open error for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: snd_pcm_hw_params error for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}
	// Test our discrete set of sample rate values.
	info.sampleRates.clear();
	for (uint32_t i=0; i<MAX_SAMPLE_RATES; i++) {
		if (snd_pcm_hw_params_test_rate(phandle, params, SAMPLE_RATES[i], 0) == 0) {
			info.sampleRates.push_back(SAMPLE_RATES[i]);
		}
	}
	if (info.sampleRates.size() == 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: no supported sample rates found for device (" << name << ").";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}
	// Probe the supported data formats ... we don't care about endian-ness just yet
	snd_pcm_format_t format;
	info.nativeFormats = 0;
	format = SND_PCM_FORMAT_S8;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats |= airtaudio::SINT8;
	}
	format = SND_PCM_FORMAT_S16;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats |= airtaudio::SINT16;
	}
	format = SND_PCM_FORMAT_S24;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats |= airtaudio::SINT24;
	}
	format = SND_PCM_FORMAT_S32;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats |= airtaudio::SINT32;
	}
	format = SND_PCM_FORMAT_FLOAT;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats |= airtaudio::FLOAT32;
	}
	format = SND_PCM_FORMAT_FLOAT64;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats |= airtaudio::FLOAT64;
	}
	// Check that we have at least one supported format
	if (info.nativeFormats == 0) {
		m_errorStream << "airtaudio::api::Alsa::getDeviceInfo: pcm device (" << name << ") data format not supported by RtAudio.";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}
	// Get the device name
	char *cardname;
	result = snd_card_get_name(card, &cardname);
	if (result >= 0) {
		sprintf(name, "hw:%s,%d", cardname, subdevice);
	}
	info.name = name;
	// That's all ... close the device and return
	snd_pcm_close(phandle);
	info.probed = true;
	return info;
}

void airtaudio::api::Alsa::saveDeviceInfo(void) {
	m_devices.clear();
	uint32_t nDevices = getDeviceCount();
	m_devices.resize(nDevices);
	for (uint32_t iii=0; iii<nDevices; ++iii) {
		m_devices[iii] = getDeviceInfo(iii);
	}
}

bool airtaudio::api::Alsa::probeDeviceOpen(uint32_t _device,
                                           airtaudio::api::StreamMode _mode,
                                           uint32_t _channels,
                                           uint32_t _firstChannel,
                                           uint32_t _sampleRate,
                                           airtaudio::format _format,
                                           uint32_t *_bufferSize,
                                           airtaudio::StreamOptions *_options) {
#if defined(__RTAUDIO_DEBUG__)
	snd_output_t *out;
	snd_output_stdio_attach(&out, stderr, 0);
#endif
	// I'm not using the "plug" interface ... too much inconsistent behavior.
	unsigned nDevices = 0;
	int32_t result, subdevice, card;
	char name[64];
	snd_ctl_t *chandle;
	if (_options && _options->flags & airtaudio::ALSA_USE_DEFAULT) {
		snprintf(name, sizeof(name), "%s", "default");
	} else {
		// Count cards and devices
		card = -1;
		snd_card_next(&card);
		while (card >= 0) {
			sprintf(name, "hw:%d", card);
			result = snd_ctl_open(&chandle, name, SND_CTL_NONBLOCK);
			if (result < 0) {
				m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: control open, card = " << card << ", " << snd_strerror(result) << ".";
				m_errorText = m_errorStream.str();
				return FAILURE;
			}
			subdevice = -1;
			while(1) {
				result = snd_ctl_pcm_next_device(chandle, &subdevice);
				if (result < 0) break;
				if (subdevice < 0) break;
				if (nDevices == _device) {
					sprintf(name, "hw:%d,%d", card, subdevice);
					snd_ctl_close(chandle);
					goto foundDevice;
				}
				nDevices++;
			}
			snd_ctl_close(chandle);
			snd_card_next(&card);
		}
		result = snd_ctl_open(&chandle, "default", SND_CTL_NONBLOCK);
		if (result == 0) {
			if (nDevices == _device) {
				strcpy(name, "default");
				goto foundDevice;
			}
			nDevices++;
		}
		if (nDevices == 0) {
			// This should not happen because a check is made before this function is called.
			m_errorText = "airtaudio::api::Alsa::probeDeviceOpen: no devices found!";
			return FAILURE;
		}
		if (_device >= nDevices) {
			// This should not happen because a check is made before this function is called.
			m_errorText = "airtaudio::api::Alsa::probeDeviceOpen: device ID is invalid!";
			return FAILURE;
		}
	}

foundDevice:
	// The getDeviceInfo() function will not work for a device that is
	// already open.	Thus, we'll probe the system before opening a
	// stream and save the results for use by getDeviceInfo().
	if (    _mode == OUTPUT
	    || (    _mode == INPUT
	         && m_stream.mode != OUTPUT)) {
		// only do once
		this->saveDeviceInfo();
	}
	snd_pcm_stream_t stream;
	if (_mode == OUTPUT) {
		stream = SND_PCM_STREAM_PLAYBACK;
	} else {
		stream = SND_PCM_STREAM_CAPTURE;
	}
	snd_pcm_t *phandle;
	int32_t openMode = SND_PCM_ASYNC;
	result = snd_pcm_open(&phandle, name, stream, openMode);
	if (result < 0) {
		if (_mode == OUTPUT) {
			m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: pcm device (" << name << ") won't open for output.";
		} else {
			m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: pcm device (" << name << ") won't open for input.";
		}
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	// Fill the parameter structure.
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_hw_params_alloca(&hw_params);
	result = snd_pcm_hw_params_any(phandle, hw_params);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error getting pcm device (" << name << ") parameters, " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
#if defined(__RTAUDIO_DEBUG__)
	fprintf(stderr, "\nRtApiAlsa: dump hardware params just after device open:\n\n");
	snd_pcm_hw_params_dump(hw_params, out);
#endif
	// Set access ... check user preference.
	if (    _options != NULL
	     && _options->flags & airtaudio::NONINTERLEAVED) {
		m_stream.userInterleaved = false;
		result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
		if (result < 0) {
			result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
			m_stream.deviceInterleaved[_mode] =	true;
		} else {
			m_stream.deviceInterleaved[_mode] = false;
		}
	} else {
		m_stream.userInterleaved = true;
		result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
		if (result < 0) {
			result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
			m_stream.deviceInterleaved[_mode] =	false;
		} else {
			m_stream.deviceInterleaved[_mode] =	true;
		}
	}
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error setting pcm device (" << name << ") access, " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	// Determine how to set the device format.
	m_stream.userFormat = _format;
	snd_pcm_format_t deviceFormat = SND_PCM_FORMAT_UNKNOWN;
	if (_format == airtaudio::SINT8) {
		deviceFormat = SND_PCM_FORMAT_S8;
	} else if (_format == airtaudio::SINT16) {
		deviceFormat = SND_PCM_FORMAT_S16;
	} else if (_format == airtaudio::SINT24) {
		deviceFormat = SND_PCM_FORMAT_S24;
	} else if (_format == airtaudio::SINT32) {
		deviceFormat = SND_PCM_FORMAT_S32;
	} else if (_format == airtaudio::FLOAT32) {
		deviceFormat = SND_PCM_FORMAT_FLOAT;
	} else if (_format == airtaudio::FLOAT64) {
		deviceFormat = SND_PCM_FORMAT_FLOAT64;
	}
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_stream.deviceFormat[_mode] = _format;
		goto setFormat;
	}
	// The user requested format is not natively supported by the device.
	deviceFormat = SND_PCM_FORMAT_FLOAT64;
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_stream.deviceFormat[_mode] = airtaudio::FLOAT64;
		goto setFormat;
	}
	deviceFormat = SND_PCM_FORMAT_FLOAT;
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_stream.deviceFormat[_mode] = airtaudio::FLOAT32;
		goto setFormat;
	}
	deviceFormat = SND_PCM_FORMAT_S32;
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_stream.deviceFormat[_mode] = airtaudio::SINT32;
		goto setFormat;
	}
	deviceFormat = SND_PCM_FORMAT_S24;
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_stream.deviceFormat[_mode] = airtaudio::SINT24;
		goto setFormat;
	}
	deviceFormat = SND_PCM_FORMAT_S16;
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_stream.deviceFormat[_mode] = airtaudio::SINT16;
		goto setFormat;
	}
	deviceFormat = SND_PCM_FORMAT_S8;
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_stream.deviceFormat[_mode] = airtaudio::SINT8;
		goto setFormat;
	}
	// If we get here, no supported format was found.
	snd_pcm_close(phandle);
	m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: pcm device " << _device << " data format not supported by RtAudio.";
	m_errorText = m_errorStream.str();
	return FAILURE;

setFormat:
	result = snd_pcm_hw_params_set_format(phandle, hw_params, deviceFormat);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error setting pcm device (" << name << ") data format, " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	// Determine whether byte-swaping is necessary.
	m_stream.doByteSwap[_mode] = false;
	if (deviceFormat != SND_PCM_FORMAT_S8) {
		result = snd_pcm_format_cpu_endian(deviceFormat);
		if (result == 0) {
			m_stream.doByteSwap[_mode] = true;
		} else if (result < 0) {
			snd_pcm_close(phandle);
			m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error getting pcm device (" << name << ") endian-ness, " << snd_strerror(result) << ".";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}
	}
	// Set the sample rate.
	result = snd_pcm_hw_params_set_rate_near(phandle, hw_params, (uint32_t*) &_sampleRate, 0);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error setting sample rate on device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	// Determine the number of channels for this device.	We support a possible
	// minimum device channel number > than the value requested by the user.
	m_stream.nUserChannels[_mode] = _channels;
	uint32_t value;
	result = snd_pcm_hw_params_get_channels_max(hw_params, &value);
	uint32_t deviceChannels = value;
	if (    result < 0
	     || deviceChannels < _channels + _firstChannel) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: requested channel parameters not supported by device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	result = snd_pcm_hw_params_get_channels_min(hw_params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error getting minimum channels for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	deviceChannels = value;
	if (deviceChannels < _channels + _firstChannel) {
		deviceChannels = _channels + _firstChannel;
	}
	m_stream.nDeviceChannels[_mode] = deviceChannels;
	// Set the device channels.
	result = snd_pcm_hw_params_set_channels(phandle, hw_params, deviceChannels);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error setting channels for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	// Set the buffer (or period) size.
	int32_t dir = 0;
	snd_pcm_uframes_t periodSize = *_bufferSize;
	result = snd_pcm_hw_params_set_period_size_near(phandle, hw_params, &periodSize, &dir);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error setting period size for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	*_bufferSize = periodSize;
	// Set the buffer number, which in ALSA is referred to as the "period".
	uint32_t periods = 0;
	if (_options && _options->flags & airtaudio::MINIMIZE_LATENCY) periods = 2;
	if (_options && _options->numberOfBuffers > 0) periods = _options->numberOfBuffers;
	if (periods < 2) periods = 4; // a fairly safe default value
	result = snd_pcm_hw_params_set_periods_near(phandle, hw_params, &periods, &dir);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error setting periods for device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	// If attempting to setup a duplex stream, the bufferSize parameter
	// MUST be the same in both directions!
	if (m_stream.mode == OUTPUT && _mode == INPUT && *_bufferSize != m_stream.bufferSize) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: system error setting buffer size for duplex stream on device (" << name << ").";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
	m_stream.bufferSize = *_bufferSize;
	// Install the hardware configuration
	result = snd_pcm_hw_params(phandle, hw_params);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error installing hardware configuration on device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
#if defined(__RTAUDIO_DEBUG__)
	fprintf(stderr, "\nRtApiAlsa: dump hardware params after installation:\n\n");
	snd_pcm_hw_params_dump(hw_params, out);
#endif
	// Set the software configuration to fill buffers with zeros and prevent device stopping on xruns.
	snd_pcm_sw_params_t *sw_params = NULL;
	snd_pcm_sw_params_alloca(&sw_params);
	snd_pcm_sw_params_current(phandle, sw_params);
	snd_pcm_sw_params_set_start_threshold(phandle, sw_params, *_bufferSize);
	snd_pcm_sw_params_set_stop_threshold(phandle, sw_params, ULONG_MAX);
	snd_pcm_sw_params_set_silence_threshold(phandle, sw_params, 0);
	// The following two settings were suggested by Theo Veenker
	//snd_pcm_sw_params_set_avail_min(phandle, sw_params, *_bufferSize);
	//snd_pcm_sw_params_set_xfer_align(phandle, sw_params, 1);
	// here are two options for a fix
	//snd_pcm_sw_params_set_silence_size(phandle, sw_params, ULONG_MAX);
	snd_pcm_uframes_t val;
	snd_pcm_sw_params_get_boundary(sw_params, &val);
	snd_pcm_sw_params_set_silence_size(phandle, sw_params, val);
	result = snd_pcm_sw_params(phandle, sw_params);
	if (result < 0) {
		snd_pcm_close(phandle);
		m_errorStream << "airtaudio::api::Alsa::probeDeviceOpen: error installing software configuration on device (" << name << "), " << snd_strerror(result) << ".";
		m_errorText = m_errorStream.str();
		return FAILURE;
	}
#if defined(__RTAUDIO_DEBUG__)
	fprintf(stderr, "\nRtApiAlsa: dump software params after installation:\n\n");
	snd_pcm_sw_params_dump(sw_params, out);
#endif
	// Set flags for buffer conversion
	m_stream.doConvertBuffer[_mode] = false;
	if (m_stream.userFormat != m_stream.deviceFormat[_mode]) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (m_stream.nUserChannels[_mode] < m_stream.nDeviceChannels[_mode]) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (    m_stream.userInterleaved != m_stream.deviceInterleaved[_mode]
	     && m_stream.nUserChannels[_mode] > 1) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	// Allocate the ApiHandle if necessary and then save.
	AlsaHandle *apiInfo = 0;
	if (m_stream.apiHandle == 0) {
		try {
			apiInfo = (AlsaHandle *) new AlsaHandle;
		}
		catch (std::bad_alloc&) {
			m_errorText = "airtaudio::api::Alsa::probeDeviceOpen: error allocating AlsaHandle memory.";
			goto error;
		}
		m_stream.apiHandle = (void *) apiInfo;
		apiInfo->handles[0] = 0;
		apiInfo->handles[1] = 0;
	} else {
		apiInfo = (AlsaHandle *) m_stream.apiHandle;
	}
	apiInfo->handles[_mode] = phandle;
	phandle = 0;
	// Allocate necessary internal buffers.
	uint64_t bufferBytes;
	bufferBytes = m_stream.nUserChannels[_mode] * *_bufferSize * formatBytes(m_stream.userFormat);
	m_stream.userBuffer[_mode] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[_mode] == NULL) {
		m_errorText = "airtaudio::api::Alsa::probeDeviceOpen: error allocating user buffer memory.";
		goto error;
	}
	if (m_stream.doConvertBuffer[_mode]) {
		bool makeBuffer = true;
		bufferBytes = m_stream.nDeviceChannels[_mode] * formatBytes(m_stream.deviceFormat[_mode]);
		if (_mode == INPUT) {
			if (m_stream.mode == OUTPUT && m_stream.deviceBuffer) {
				uint64_t bytesOut = m_stream.nDeviceChannels[0] * formatBytes(m_stream.deviceFormat[0]);
				if (bufferBytes <= bytesOut) {
					makeBuffer = false;
				}
			}
		}
		if (makeBuffer) {
			bufferBytes *= *_bufferSize;
			if (m_stream.deviceBuffer) {
				free(m_stream.deviceBuffer);
				m_stream.deviceBuffer = NULL;
			}
			m_stream.deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_stream.deviceBuffer == NULL) {
				m_errorText = "airtaudio::api::Alsa::probeDeviceOpen: error allocating device buffer memory.";
				goto error;
			}
		}
	}
	m_stream.sampleRate = _sampleRate;
	m_stream.nBuffers = periods;
	m_stream.device[_mode] = _device;
	m_stream.state = STREAM_STOPPED;
	// Setup the buffer conversion information structure.
	if (m_stream.doConvertBuffer[_mode]) {
		setConvertInfo(_mode, _firstChannel);
	}
	// Setup thread if necessary.
	if (    m_stream.mode == OUTPUT
	     && _mode == INPUT) {
		// We had already set up an output stream.
		m_stream.mode = DUPLEX;
		// Link the streams if possible.
		apiInfo->synchronized = false;
		if (snd_pcm_link(apiInfo->handles[0], apiInfo->handles[1]) == 0) {
			apiInfo->synchronized = true;
		} else {
			m_errorText = "airtaudio::api::Alsa::probeDeviceOpen: unable to synchronize input and output devices.";
			error(airtaudio::errorWarning);
		}
	} else {
		m_stream.mode = _mode;
		// Setup callback thread.
		m_stream.callbackInfo.object = (void *) this;
		m_stream.callbackInfo.isRunning = true;
		m_stream.callbackInfo.thread = new std::thread(alsaCallbackHandler, &m_stream.callbackInfo);
		if (m_stream.callbackInfo.thread == NULL) {
			m_stream.callbackInfo.isRunning = false;
			m_errorText = "airtaudio::api::Alsa::error creating callback thread!";
			goto error;
		}
	}
	return SUCCESS;
error:
	if (apiInfo != NULL) {
		if (apiInfo->handles[0]) {
			snd_pcm_close(apiInfo->handles[0]);
		}
		if (apiInfo->handles[1]) {
			snd_pcm_close(apiInfo->handles[1]);
		}
		delete apiInfo;
		apiInfo = NULL;
		m_stream.apiHandle = 0;
	}
	if (phandle) {
		snd_pcm_close(phandle);
	}
	for (int32_t iii=0; iii<2; ++iii) {
		if (m_stream.userBuffer[iii]) {
			free(m_stream.userBuffer[iii]);
			m_stream.userBuffer[iii] = 0;
		}
	}
	if (m_stream.deviceBuffer) {
		free(m_stream.deviceBuffer);
		m_stream.deviceBuffer = 0;
	}
	m_stream.state = STREAM_CLOSED;
	return FAILURE;
}

enum airtaudio::errorType airtaudio::api::Alsa::closeStream(void) {
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Alsa::closeStream(): no open stream to close!";
		error(airtaudio::errorWarning);
		return;
	}
	AlsaHandle *apiInfo = (AlsaHandle *) m_stream.apiHandle;
	m_stream.callbackInfo.isRunning = false;
	m_stream.mutex.lock();
	if (m_stream.state == STREAM_STOPPED) {
		apiInfo->runnable = true;
		apiInfo->runnable_cv.notify_one();
	}
	m_stream.mutex.unlock();
	if (m_stream.callbackInfo.thread != NULL) {
		m_stream.callbackInfo.thread->join();
	}
	if (m_stream.state == STREAM_RUNNING) {
		m_stream.state = STREAM_STOPPED;
		if (    m_stream.mode == OUTPUT
		     || m_stream.mode == DUPLEX) {
			snd_pcm_drop(apiInfo->handles[0]);
		}
		if (    m_stream.mode == INPUT
		     || m_stream.mode == DUPLEX) {
			snd_pcm_drop(apiInfo->handles[1]);
		}
	}
	if (apiInfo != NULL) {
		if (apiInfo->handles[0]) {
			snd_pcm_close(apiInfo->handles[0]);
		}
		if (apiInfo->handles[1]) {
			snd_pcm_close(apiInfo->handles[1]);
		}
		delete apiInfo;
		apiInfo = NULL;
		m_stream.apiHandle = 0;
	}
	for (int32_t iii=0; iii<2; ++iii) {
		if (m_stream.userBuffer[iii] != NULL) {
			free(m_stream.userBuffer[iii]);
			m_stream.userBuffer[iii] = 0;
		}
	}
	if (m_stream.deviceBuffer) {
		free(m_stream.deviceBuffer);
		m_stream.deviceBuffer = 0;
	}
	m_stream.mode = UNINITIALIZED;
	m_stream.state = STREAM_CLOSED;
}

enum airtaudio::errorType airtaudio::api::Alsa::startStream(void) {
	// This method calls snd_pcm_prepare if the device isn't already in that state.
	verifyStream();
	if (m_stream.state == STREAM_RUNNING) {
		m_errorText = "airtaudio::api::Alsa::startStream(): the stream is already running!";
		error(airtaudio::errorWarning);
		return;
	}
	std::unique_lock<std::mutex> lck(m_stream.mutex);
	int32_t result = 0;
	snd_pcm_state_t state;
	AlsaHandle *apiInfo = (AlsaHandle *) m_stream.apiHandle;
	snd_pcm_t **handle = (snd_pcm_t **) apiInfo->handles;
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {
		state = snd_pcm_state(handle[0]);
		if (state != SND_PCM_STATE_PREPARED) {
			result = snd_pcm_prepare(handle[0]);
			if (result < 0) {
				m_errorStream << "airtaudio::api::Alsa::startStream: error preparing output pcm device, " << snd_strerror(result) << ".";
				m_errorText = m_errorStream.str();
				goto unlock;
			}
		}
	}
	if (    (    m_stream.mode == INPUT
	          || m_stream.mode == DUPLEX)
	     && !apiInfo->synchronized) {
		state = snd_pcm_state(handle[1]);
		if (state != SND_PCM_STATE_PREPARED) {
			result = snd_pcm_prepare(handle[1]);
			if (result < 0) {
				m_errorStream << "airtaudio::api::Alsa::startStream: error preparing input pcm device, " << snd_strerror(result) << ".";
				m_errorText = m_errorStream.str();
				goto unlock;
			}
		}
	}
	m_stream.state = STREAM_RUNNING;
unlock:
	apiInfo->runnable = true;
	apiInfo->runnable_cv.notify_one();
	if (result >= 0) {
		return;
	}
	error(airtaudio::errorSystemError);
}

enum airtaudio::errorType airtaudio::api::Alsa::stopStream(void) {
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Alsa::stopStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}
	m_stream.state = STREAM_STOPPED;
	std::unique_lock<std::mutex> lck(m_stream.mutex);
	int32_t result = 0;
	AlsaHandle *apiInfo = (AlsaHandle *) m_stream.apiHandle;
	snd_pcm_t **handle = (snd_pcm_t **) apiInfo->handles;
	if (    m_stream.mode == OUTPUT
	     || m_stream.mode == DUPLEX) {
		if (apiInfo->synchronized) {
			result = snd_pcm_drop(handle[0]);
		} else {
			result = snd_pcm_drain(handle[0]);
		}
		if (result < 0) {
			m_errorStream << "airtaudio::api::Alsa::stopStream: error draining output pcm device, " << snd_strerror(result) << ".";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}
	if (    (    m_stream.mode == INPUT
	          || m_stream.mode == DUPLEX)
	     && !apiInfo->synchronized) {
		result = snd_pcm_drop(handle[1]);
		if (result < 0) {
			m_errorStream << "airtaudio::api::Alsa::stopStream: error stopping input pcm device, " << snd_strerror(result) << ".";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}
unlock:
	if (result >= 0) {
		return;
	}
	error(airtaudio::errorSystemError);
}

enum airtaudio::errorType airtaudio::api::Alsa::abortStream(void) {
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Alsa::abortStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}
	m_stream.state = STREAM_STOPPED;
	std::unique_lock<std::mutex> lck(m_stream.mutex);
	int32_t result = 0;
	AlsaHandle *apiInfo = (AlsaHandle *) m_stream.apiHandle;
	snd_pcm_t **handle = (snd_pcm_t **) apiInfo->handles;
	if (    m_stream.mode == OUTPUT
	     || m_stream.mode == DUPLEX) {
		result = snd_pcm_drop(handle[0]);
		if (result < 0) {
			m_errorStream << "airtaudio::api::Alsa::abortStream: error aborting output pcm device, " << snd_strerror(result) << ".";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}
	if (    (    m_stream.mode == INPUT
	          || m_stream.mode == DUPLEX)
	     && !apiInfo->synchronized) {
		result = snd_pcm_drop(handle[1]);
		if (result < 0) {
			m_errorStream << "airtaudio::api::Alsa::abortStream: error aborting input pcm device, " << snd_strerror(result) << ".";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}
unlock:
	if (result >= 0) {
		return;
	}
	error(airtaudio::errorSystemError);
}

void airtaudio::api::Alsa::callbackEvent(void) {
	AlsaHandle *apiInfo = (AlsaHandle *) m_stream.apiHandle;
	if (m_stream.state == STREAM_STOPPED) {
		std::unique_lock<std::mutex> lck(m_stream.mutex);
		// TODO : Set this back ....
		/*
		while (!apiInfo->runnable) {
			apiInfo->runnable_cv.wait(lck);
		}
		*/
		if (m_stream.state != STREAM_RUNNING) {
			return;
		}
	}
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Alsa::callbackEvent(): the stream is closed ... this shouldn't happen!";
		error(airtaudio::errorWarning);
		return;
	}
	int32_t doStopStream = 0;
	airtaudio::AirTAudioCallback callback = (airtaudio::AirTAudioCallback) m_stream.callbackInfo.callback;
	double streamTime = getStreamTime();
	airtaudio::streamStatus status = 0;
	if (m_stream.mode != INPUT && apiInfo->xrun[0] == true) {
		status |= airtaudio::OUTPUT_UNDERFLOW;
		apiInfo->xrun[0] = false;
	}
	if (m_stream.mode != OUTPUT && apiInfo->xrun[1] == true) {
		status |= airtaudio::INPUT_OVERFLOW;
		apiInfo->xrun[1] = false;
	}
	doStopStream = callback(m_stream.userBuffer[0],
	                        m_stream.userBuffer[1],
	                        m_stream.bufferSize,
	                        streamTime,
	                        status,
	                        m_stream.callbackInfo.userData);
	if (doStopStream == 2) {
		abortStream();
		return;
	}
	std::unique_lock<std::mutex> lck(m_stream.mutex);
	// The state might change while waiting on a mutex.
	if (m_stream.state == STREAM_STOPPED) {
		goto unlock;
	}
	int32_t result;
	char *buffer;
	int32_t channels;
	snd_pcm_t **handle;
	snd_pcm_sframes_t frames;
	airtaudio::format format;
	handle = (snd_pcm_t **) apiInfo->handles;
	if (    m_stream.mode == airtaudio::api::INPUT
	     || m_stream.mode == airtaudio::api::DUPLEX) {
		// Setup parameters.
		if (m_stream.doConvertBuffer[1]) {
			buffer = m_stream.deviceBuffer;
			channels = m_stream.nDeviceChannels[1];
			format = m_stream.deviceFormat[1];
		} else {
			buffer = m_stream.userBuffer[1];
			channels = m_stream.nUserChannels[1];
			format = m_stream.userFormat;
		}
		// Read samples from device in interleaved/non-interleaved format.
		if (m_stream.deviceInterleaved[1]) {
			result = snd_pcm_readi(handle[1], buffer, m_stream.bufferSize);
		} else {
			void *bufs[channels];
			size_t offset = m_stream.bufferSize * formatBytes(format);
			for (int32_t i=0; i<channels; i++)
				bufs[i] = (void *) (buffer + (i * offset));
			result = snd_pcm_readn(handle[1], bufs, m_stream.bufferSize);
		}
		if (result < (int) m_stream.bufferSize) {
			// Either an error or overrun occured.
			if (result == -EPIPE) {
				snd_pcm_state_t state = snd_pcm_state(handle[1]);
				if (state == SND_PCM_STATE_XRUN) {
					apiInfo->xrun[1] = true;
					result = snd_pcm_prepare(handle[1]);
					if (result < 0) {
						m_errorStream << "airtaudio::api::Alsa::callbackEvent: error preparing device after overrun, " << snd_strerror(result) << ".";
						m_errorText = m_errorStream.str();
					}
				} else {
					m_errorStream << "airtaudio::api::Alsa::callbackEvent: error, current state is " << snd_pcm_state_name(state) << ", " << snd_strerror(result) << ".";
					m_errorText = m_errorStream.str();
				}
			} else {
				m_errorStream << "airtaudio::api::Alsa::callbackEvent: audio read error, " << snd_strerror(result) << ".";
				m_errorText = m_errorStream.str();
			}
			error(airtaudio::errorWarning);
			goto tryOutput;
		}
		// Do byte swapping if necessary.
		if (m_stream.doByteSwap[1]) {
			byteSwapBuffer(buffer, m_stream.bufferSize * channels, format);
		}
		// Do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[1]) {
			convertBuffer(m_stream.userBuffer[1], m_stream.deviceBuffer, m_stream.convertInfo[1]);
		}
		// Check stream latency
		result = snd_pcm_delay(handle[1], &frames);
		if (result == 0 && frames > 0) m_stream.latency[1] = frames;
	}

tryOutput:
	if (    m_stream.mode == airtaudio::api::OUTPUT
	     || m_stream.mode == airtaudio::api::DUPLEX) {
		// Setup parameters and do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[0]) {
			buffer = m_stream.deviceBuffer;
			convertBuffer(buffer, m_stream.userBuffer[0], m_stream.convertInfo[0]);
			channels = m_stream.nDeviceChannels[0];
			format = m_stream.deviceFormat[0];
		} else {
			buffer = m_stream.userBuffer[0];
			channels = m_stream.nUserChannels[0];
			format = m_stream.userFormat;
		}
		// Do byte swapping if necessary.
		if (m_stream.doByteSwap[0]) {
			byteSwapBuffer(buffer, m_stream.bufferSize * channels, format);
		}
		// Write samples to device in interleaved/non-interleaved format.
		if (m_stream.deviceInterleaved[0]) {
			result = snd_pcm_writei(handle[0], buffer, m_stream.bufferSize);
		} else {
			void *bufs[channels];
			size_t offset = m_stream.bufferSize * formatBytes(format);
			for (int32_t i=0; i<channels; i++) {
				bufs[i] = (void *) (buffer + (i * offset));
			}
			result = snd_pcm_writen(handle[0], bufs, m_stream.bufferSize);
		}
		if (result < (int) m_stream.bufferSize) {
			// Either an error or underrun occured.
			if (result == -EPIPE) {
				snd_pcm_state_t state = snd_pcm_state(handle[0]);
				if (state == SND_PCM_STATE_XRUN) {
					apiInfo->xrun[0] = true;
					result = snd_pcm_prepare(handle[0]);
					if (result < 0) {
						m_errorStream << "airtaudio::api::Alsa::callbackEvent: error preparing device after underrun, " << snd_strerror(result) << ".";
						m_errorText = m_errorStream.str();
					}
				} else {
					m_errorStream << "airtaudio::api::Alsa::callbackEvent: error, current state is " << snd_pcm_state_name(state) << ", " << snd_strerror(result) << ".";
					m_errorText = m_errorStream.str();
				}
			} else {
				m_errorStream << "airtaudio::api::Alsa::callbackEvent: audio write error, " << snd_strerror(result) << ".";
				m_errorText = m_errorStream.str();
			}
			error(airtaudio::errorWarning);
			goto unlock;
		}
		// Check stream latency
		result = snd_pcm_delay(handle[0], &frames);
		if (result == 0 && frames > 0) m_stream.latency[0] = frames;
	}

unlock:
	airtaudio::Api::tickStreamTime();
	if (doStopStream == 1) {
		this->stopStream();
	}
}

static void alsaCallbackHandler(void *_ptr) {
	airtaudio::CallbackInfo *info = (airtaudio::CallbackInfo*)_ptr;
	airtaudio::api::Alsa *object = (airtaudio::api::Alsa *) info->object;
	bool *isRunning = &info->isRunning;
	while (*isRunning == true) {
		// TODO : pthread_testcancel();
		object->callbackEvent();
	}
}

#endif
