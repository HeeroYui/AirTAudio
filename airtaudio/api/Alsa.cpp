/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */


#if defined(__LINUX_ALSA__)

#include <alsa/asoundlib.h>
#include <unistd.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <limits.h>

#undef __class__
#define __class__ "api::Alsa"

airtaudio::Api* airtaudio::api::Alsa::Create() {
	return new airtaudio::api::Alsa();
}

namespace airtaudio {
	namespace api {
		class AlsaPrivate {
			public:
				snd_pcm_t *handles[2];
				bool synchronized;
				bool xrun[2];
				std11::condition_variable runnable_cv;
				bool runnable;
				std11::thread* thread;
				bool threadRunning;
				AlsaPrivate() :
				  synchronized(false),
				  runnable(false),
				  thread(nullptr),
				  threadRunning(false) {
					handles[0] = nullptr;
					handles[1] = nullptr;
					xrun[0] = false;
					xrun[1] = false;
					// TODO : Wait thread ...
				}
		};
	};
};

airtaudio::api::Alsa::Alsa() :
  m_private(new airtaudio::api::AlsaPrivate()) {
	// Nothing to do here.
}

airtaudio::api::Alsa::~Alsa() {
	if (m_state != airtaudio::state_closed) {
		closeStream();
	}
}

uint32_t airtaudio::api::Alsa::getDeviceCount() {
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
			ATA_ERROR("control open, card = " << card << ", " << snd_strerror(result) << ".");
			// TODO : Return error airtaudio::error_warning;
			goto nextcard;
		}
		subdevice = -1;
		while(1) {
			result = snd_ctl_pcm_next_device(handle, &subdevice);
			if (result < 0) {
				ATA_ERROR("control next device, card = " << card << ", " << snd_strerror(result) << ".");
				// TODO : Return error airtaudio::error_warning;
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
			ATA_WARNING("control open, card = " << card << ", " << snd_strerror(result) << ".");
			goto nextcard;
		}
		subdevice = -1;
		while(1) {
			result = snd_ctl_pcm_next_device(chandle, &subdevice);
			if (result < 0) {
				ATA_WARNING("control next device, card = " << card << ", " << snd_strerror(result) << ".");
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
		ATA_ERROR("no devices found!");
		// TODO : airtaudio::error_invalidUse;
		return info;
	}
	if (_device >= nDevices) {
		ATA_ERROR("device ID is invalid!");
		// TODO : airtaudio::error_invalidUse;
		return info;
	}

foundDevice:
	// If a stream is already open, we cannot probe the stream devices.
	// Thus, use the saved results.
	if (    m_state != airtaudio::state_closed
	     && (    m_device[0] == _device
	          || m_device[1] == _device)) {
		snd_ctl_close(chandle);
		if (_device >= m_devices.size()) {
			ATA_ERROR("device ID was not present before stream was opened.");
			// TODO : return airtaudio::error_warning;
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
		ATA_ERROR("snd_pcm_open error for device (" << name << "), " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
		goto captureProbe;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("snd_pcm_hw_params error for device (" << name << "), " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
		goto captureProbe;
	}
	// Get output channel information.
	uint32_t value;
	result = snd_pcm_hw_params_get_channels_max(params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting device (" << name << ") output channels, " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
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
		ATA_ERROR("snd_pcm_open error for device (" << name << "), " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
		if (info.outputChannels == 0) {
			return info;
		}
		goto probeParameters;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("snd_pcm_hw_params error for device (" << name << "), " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
		if (info.outputChannels == 0) {
			return info;
		}
		goto probeParameters;
	}
	result = snd_pcm_hw_params_get_channels_max(params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting device (" << name << ") input channels, " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
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
		ATA_ERROR("snd_pcm_open error for device (" << name << "), " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
		return info;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("snd_pcm_hw_params error for device (" << name << "), " << snd_strerror(result) << ".");
		// TODO : Return airtaudio::error_warning;
		return info;
	}
	// Test our discrete set of sample rate values.
	info.sampleRates.clear();
	#if __CPP_VERSION__ >= 2011
		for (auto &it : airtaudio::genericSampleRate()) {
			if (snd_pcm_hw_params_test_rate(phandle, params, it, 0) == 0) {
				info.sampleRates.push_back(it);
			}
		}
	#else
		for (std::vector<uint32_t>::const_iterator it(airtaudio::genericSampleRate().begin()); 
		     it != airtaudio::genericSampleRate().end();
		     ++it ) {
			if (snd_pcm_hw_params_test_rate(phandle, params, *it, 0) == 0) {
				info.sampleRates.push_back(*it);
			}
		}
	#endif
	if (info.sampleRates.size() == 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("no supported sample rates found for device (" << name << ").");
		// TODO : Return airtaudio::error_warning;
		return info;
	}
	// Probe the supported data formats ... we don't care about endian-ness just yet
	snd_pcm_format_t format;
	info.nativeFormats.clear();
	format = SND_PCM_FORMAT_S8;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats.push_back(audio::format_int8);
	}
	format = SND_PCM_FORMAT_S16;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats.push_back(audio::format_int16);
	}
	format = SND_PCM_FORMAT_S24;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats.push_back(audio::format_int24);
	}
	format = SND_PCM_FORMAT_S32;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats.push_back(audio::format_int32);
	}
	format = SND_PCM_FORMAT_FLOAT;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats.push_back(audio::format_float);
	}
	format = SND_PCM_FORMAT_FLOAT64;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		info.nativeFormats.push_back(audio::format_double);
	}
	// Check that we have at least one supported format
	if (info.nativeFormats.size() == 0) {
		ATA_ERROR("pcm device (" << name << ") data format not supported by RtAudio.");
		// TODO : Return airtaudio::error_warning;
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

void airtaudio::api::Alsa::saveDeviceInfo() {
	m_devices.clear();
	uint32_t nDevices = getDeviceCount();
	m_devices.resize(nDevices);
	for (uint32_t iii=0; iii<nDevices; ++iii) {
		m_devices[iii] = getDeviceInfo(iii);
	}
}

bool airtaudio::api::Alsa::probeDeviceOpen(uint32_t _device,
                                           airtaudio::mode _mode,
                                           uint32_t _channels,
                                           uint32_t _firstChannel,
                                           uint32_t _sampleRate,
                                           audio::format _format,
                                           uint32_t *_bufferSize,
                                           airtaudio::StreamOptions *_options) {
	// I'm not using the "plug" interface ... too much inconsistent behavior.
	unsigned nDevices = 0;
	int32_t result, subdevice, card;
	char name[64];
	snd_ctl_t *chandle;
	// Count cards and devices
	card = -1;
	// NOTE : Find the device name : [BEGIN]
	snd_card_next(&card);
	while (card >= 0) {
		sprintf(name, "hw:%d", card);
		result = snd_ctl_open(&chandle, name, SND_CTL_NONBLOCK);
		if (result < 0) {
			ATA_ERROR("control open, card = " << card << ", " << snd_strerror(result) << ".");
			return false;
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
		ATA_ERROR("no devices found!");
		return false;
	}
	if (_device >= nDevices) {
		// This should not happen because a check is made before this function is called.
		ATA_ERROR("device ID is invalid!");
		return false;
	}
	// NOTE : Find the device name : [ END ]

foundDevice:
	// The getDeviceInfo() function will not work for a device that is
	// already open.	Thus, we'll probe the system before opening a
	// stream and save the results for use by getDeviceInfo().
	if (    _mode == airtaudio::mode_output
	    || (    _mode == airtaudio::mode_input
	         && m_mode != airtaudio::mode_output)) {
		// only do once
		this->saveDeviceInfo();
	}
	snd_pcm_stream_t stream;
	if (_mode == airtaudio::mode_output) {
		stream = SND_PCM_STREAM_PLAYBACK;
	} else {
		stream = SND_PCM_STREAM_CAPTURE;
	}
	snd_pcm_t *phandle;
	int32_t openMode = SND_PCM_ASYNC;
	result = snd_pcm_open(&phandle, name, stream, openMode);
	if (result < 0) {
		if (_mode == airtaudio::mode_output) {
			ATA_ERROR("pcm device (" << name << ") won't open for output.");
		} else {
			ATA_ERROR("pcm device (" << name << ") won't open for input.");
		}
		return false;
	}
	// Fill the parameter structure.
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_hw_params_alloca(&hw_params);
	result = snd_pcm_hw_params_any(phandle, hw_params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting pcm device (" << name << ") parameters, " << snd_strerror(result) << ".");
		return false;
	}
	// Open stream all time in interleave mode (by default): (open in non interleave if we have no choice
	result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (result < 0) {
		result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
		m_deviceInterleaved[modeToIdTable(_mode)] =	false;
	} else {
		m_deviceInterleaved[modeToIdTable(_mode)] =	true;
	}
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting pcm device (" << name << ") access, " << snd_strerror(result) << ".");
		return false;
	}
	// Determine how to set the device format.
	m_userFormat = _format;
	snd_pcm_format_t deviceFormat = SND_PCM_FORMAT_UNKNOWN;
	if (_format == audio::format_int8) {
		deviceFormat = SND_PCM_FORMAT_S8;
	} else if (_format == audio::format_int16) {
		deviceFormat = SND_PCM_FORMAT_S16;
	} else if (_format == audio::format_int24) {
		deviceFormat = SND_PCM_FORMAT_S24;
	} else if (_format == audio::format_int32) {
		deviceFormat = SND_PCM_FORMAT_S32;
	} else if (_format == audio::format_float) {
		deviceFormat = SND_PCM_FORMAT_FLOAT;
	} else if (_format == audio::format_double) {
		deviceFormat = SND_PCM_FORMAT_FLOAT64;
	}
	if (snd_pcm_hw_params_test_format(phandle, hw_params, deviceFormat) == 0) {
		m_deviceFormat[modeToIdTable(_mode)] = _format;
	} else {
		// If we get here, no supported format was found.
		snd_pcm_close(phandle);
		ATA_ERROR("pcm device " << _device << " data format not supported: " << _format);
		// TODO : display list of all supported format ..
		return false;
	}
	result = snd_pcm_hw_params_set_format(phandle, hw_params, deviceFormat);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting pcm device (" << name << ") data format, " << snd_strerror(result) << ".");
		return false;
	}
	// Determine whether byte-swaping is necessary.
	m_doByteSwap[modeToIdTable(_mode)] = false;
	if (deviceFormat != SND_PCM_FORMAT_S8) {
		result = snd_pcm_format_cpu_endian(deviceFormat);
		if (result == 0) {
			m_doByteSwap[modeToIdTable(_mode)] = true;
		} else if (result < 0) {
			snd_pcm_close(phandle);
			ATA_ERROR("error getting pcm device (" << name << ") endian-ness, " << snd_strerror(result) << ".");
			return false;
		}
	}
	// Set the sample rate.
	result = snd_pcm_hw_params_set_rate_near(phandle, hw_params, (uint32_t*) &_sampleRate, 0);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting sample rate on device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
	// Determine the number of channels for this device.	We support a possible
	// minimum device channel number > than the value requested by the user.
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	uint32_t value;
	result = snd_pcm_hw_params_get_channels_max(hw_params, &value);
	uint32_t deviceChannels = value;
	if (    result < 0
	     || deviceChannels < _channels + _firstChannel) {
		snd_pcm_close(phandle);
		ATA_ERROR("requested channel parameters not supported by device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
	result = snd_pcm_hw_params_get_channels_min(hw_params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting minimum channels for device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
	deviceChannels = value;
	if (deviceChannels < _channels + _firstChannel) {
		deviceChannels = _channels + _firstChannel;
	}
	m_nDeviceChannels[modeToIdTable(_mode)] = deviceChannels;
	// Set the device channels.
	result = snd_pcm_hw_params_set_channels(phandle, hw_params, deviceChannels);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting channels for device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
	// Set the buffer (or period) size.
	int32_t dir = 0;
	snd_pcm_uframes_t periodSize = *_bufferSize;
	result = snd_pcm_hw_params_set_period_size_near(phandle, hw_params, &periodSize, &dir);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting period size for device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
	*_bufferSize = periodSize;
	// Set the buffer number, which in ALSA is referred to as the "period".
	uint32_t periods = 0;
	if (    _options != nullptr
	     && _options->flags.m_minimizeLatency == true) {
		periods = 2;
	}
	/* TODO : Chouse the number of low level buffer ...
	if (    _options != nullptr
	     && _options->numberOfBuffers > 0) {
		periods = _options->numberOfBuffers;
	}
	*/
	if (periods < 2) {
		periods = 4; // a fairly safe default value
	}
	result = snd_pcm_hw_params_set_periods_near(phandle, hw_params, &periods, &dir);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting periods for device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
	// If attempting to setup a duplex stream, the bufferSize parameter
	// MUST be the same in both directions!
	if (    m_mode == airtaudio::mode_output
	     && _mode == airtaudio::mode_input
	     && *_bufferSize != m_bufferSize) {
		snd_pcm_close(phandle);
		ATA_ERROR("system error setting buffer size for duplex stream on device (" << name << ").");
		return false;
	}
	m_bufferSize = *_bufferSize;
	// Install the hardware configuration
	result = snd_pcm_hw_params(phandle, hw_params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error installing hardware configuration on device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
	// Set the software configuration to fill buffers with zeros and prevent device stopping on xruns.
	snd_pcm_sw_params_t *swParams = nullptr;
	snd_pcm_sw_params_alloca(&swParams);
	snd_pcm_sw_params_current(phandle, swParams);
	snd_pcm_sw_params_set_start_threshold(phandle, swParams, *_bufferSize);
	snd_pcm_sw_params_set_stop_threshold(phandle, swParams, ULONG_MAX);
	snd_pcm_sw_params_set_silence_threshold(phandle, swParams, 0);
	// The following two settings were suggested by Theo Veenker
	//snd_pcm_sw_params_set_avail_min(phandle, swParams, *_bufferSize);
	//snd_pcm_sw_params_set_xfer_align(phandle, swParams, 1);
	// here are two options for a fix
	//snd_pcm_sw_params_set_silence_size(phandle, swParams, ULONG_MAX);
	snd_pcm_sw_params_set_tstamp_mode(phandle, swParams, SND_PCM_TSTAMP_ENABLE);
	snd_pcm_uframes_t val;
	snd_pcm_sw_params_get_boundary(swParams, &val);
	snd_pcm_sw_params_set_silence_size(phandle, swParams, val);
	result = snd_pcm_sw_params(phandle, swParams);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error installing software configuration on device (" << name << "), " << snd_strerror(result) << ".");
		return false;
	}
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
	m_private->handles[modeToIdTable(_mode)] = phandle;
	phandle = 0;
	// Allocate necessary internal buffers.
	uint64_t bufferBytes;
	bufferBytes = m_nUserChannels[modeToIdTable(_mode)] * *_bufferSize * audio::getFormatBytes(m_userFormat);
	m_userBuffer[modeToIdTable(_mode)].resize(bufferBytes, 0);
	if (m_userBuffer[modeToIdTable(_mode)].size() == 0) {
		ATA_ERROR("error allocating user buffer memory.");
		goto error;
	}
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		bool makeBuffer = true;
		bufferBytes = m_nDeviceChannels[modeToIdTable(_mode)] * audio::getFormatBytes(m_deviceFormat[modeToIdTable(_mode)]);
		if (_mode == airtaudio::mode_input) {
			if (    m_mode == airtaudio::mode_output
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
				m_deviceBuffer = nullptr;
			}
			m_deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_deviceBuffer == nullptr) {
				ATA_ERROR("error allocating device buffer memory.");
				goto error;
			}
		}
	}
	m_sampleRate = _sampleRate;
	m_nBuffers = periods;
	m_device[modeToIdTable(_mode)] = _device;
	m_state = airtaudio::state_stopped;
	// Setup the buffer conversion information structure.
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		setConvertInfo(_mode, _firstChannel);
	}
	// Setup thread if necessary.
	if (    m_mode == airtaudio::mode_output
	     && _mode == airtaudio::mode_input) {
		// We had already set up an output stream.
		m_mode = airtaudio::mode_duplex;
		// Link the streams if possible.
		m_private->synchronized = false;
		if (snd_pcm_link(m_private->handles[0], m_private->handles[1]) == 0) {
			m_private->synchronized = true;
		} else {
			ATA_ERROR("unable to synchronize input and output devices.");
			// TODO : airtaudio::error_warning;
		}
	} else {
		m_mode = _mode;
		// Setup callback thread.
		m_private->threadRunning = true;
		m_private->thread = new std11::thread(&airtaudio::api::Alsa::alsaCallbackEvent, this);
		if (m_private->thread == nullptr) {
			m_private->threadRunning = false;
			ATA_ERROR("creating callback thread!");
			goto error;
		}
	}
	return true;
error:
	if (m_private->handles[0]) {
		snd_pcm_close(m_private->handles[0]);
		m_private->handles[0] = nullptr;
	}
	if (m_private->handles[1]) {
		snd_pcm_close(m_private->handles[1]);
		m_private->handles[1] = nullptr;
	}
	if (phandle) {
		snd_pcm_close(phandle);
	}
	for (int32_t iii=0; iii<2; ++iii) {
		m_userBuffer[iii].clear();
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	m_state = airtaudio::state_closed;
	return false;
}

enum airtaudio::error airtaudio::api::Alsa::closeStream() {
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("no open stream to close!");
		return airtaudio::error_warning;
	}
	m_private->threadRunning = false;
	m_mutex.lock();
	if (m_state == airtaudio::state_stopped) {
		m_private->runnable = true;
		m_private->runnable_cv.notify_one();
	}
	m_mutex.unlock();
	if (m_private->thread != nullptr) {
		m_private->thread->join();
		m_private->thread = nullptr;
	}
	if (m_state == airtaudio::state_running) {
		m_state = airtaudio::state_stopped;
		if (    m_mode == airtaudio::mode_output
		     || m_mode == airtaudio::mode_duplex) {
			snd_pcm_drop(m_private->handles[0]);
		}
		if (    m_mode == airtaudio::mode_input
		     || m_mode == airtaudio::mode_duplex) {
			snd_pcm_drop(m_private->handles[1]);
		}
	}
	// close all stream :
	if (m_private->handles[0]) {
		snd_pcm_close(m_private->handles[0]);
		m_private->handles[0] = nullptr;
	}
	if (m_private->handles[1]) {
		snd_pcm_close(m_private->handles[1]);
		m_private->handles[1] = nullptr;
	}
	for (int32_t iii=0; iii<2; ++iii) {
		m_userBuffer[iii].clear();
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	m_mode = airtaudio::mode_unknow;
	m_state = airtaudio::state_closed;
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::api::Alsa::startStream() {
	// TODO : Check return ...
	//airtaudio::Api::startStream();
	// This method calls snd_pcm_prepare if the device isn't already in that state.
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_state == airtaudio::state_running) {
		ATA_ERROR("the stream is already running!");
		return airtaudio::error_warning;
	}
	std11::unique_lock<std11::mutex> lck(m_mutex);
	int32_t result = 0;
	snd_pcm_state_t state;
	snd_pcm_t **handle = (snd_pcm_t **) m_private->handles;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		if (handle[0] == nullptr) {
			ATA_ERROR("send nullptr to alsa ...");
			if (handle[1] != nullptr) {
				ATA_ERROR("note : 1 is not null");
			}
		}
		state = snd_pcm_state(handle[0]);
		if (state != SND_PCM_STATE_PREPARED) {
			result = snd_pcm_prepare(handle[0]);
			if (result < 0) {
				ATA_ERROR("error preparing output pcm device, " << snd_strerror(result) << ".");
				goto unlock;
			}
		}
	}
	if (    (    m_mode == airtaudio::mode_input
	          || m_mode == airtaudio::mode_duplex)
	     && !m_private->synchronized) {
		if (handle[1] == nullptr) {
			ATA_ERROR("send nullptr to alsa ...");
			if (handle[0] != nullptr) {
				ATA_ERROR("note : 0 is not null");
			}
		}
		state = snd_pcm_state(handle[1]);
		if (state != SND_PCM_STATE_PREPARED) {
			result = snd_pcm_prepare(handle[1]);
			if (result < 0) {
				ATA_ERROR("error preparing input pcm device, " << snd_strerror(result) << ".");
				goto unlock;
			}
		}
	}
	m_state = airtaudio::state_running;
unlock:
	m_private->runnable = true;
	m_private->runnable_cv.notify_one();
	if (result >= 0) {
		return airtaudio::error_none;
	}
	return airtaudio::error_systemError;
}

enum airtaudio::error airtaudio::api::Alsa::stopStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return airtaudio::error_warning;
	}
	m_state = airtaudio::state_stopped;
	std11::unique_lock<std11::mutex> lck(m_mutex);
	int32_t result = 0;
	snd_pcm_t **handle = (snd_pcm_t **) m_private->handles;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		if (m_private->synchronized) {
			result = snd_pcm_drop(handle[0]);
		} else {
			result = snd_pcm_drain(handle[0]);
		}
		if (result < 0) {
			ATA_ERROR("error draining output pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
	if (    (    m_mode == airtaudio::mode_input
	          || m_mode == airtaudio::mode_duplex)
	     && !m_private->synchronized) {
		result = snd_pcm_drop(handle[1]);
		if (result < 0) {
			ATA_ERROR("error stopping input pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
unlock:
	if (result >= 0) {
		return airtaudio::error_none;
	}
	return airtaudio::error_systemError;
}

enum airtaudio::error airtaudio::api::Alsa::abortStream() {
	if (verifyStream() != airtaudio::error_none) {
		return airtaudio::error_fail;
	}
	if (m_state == airtaudio::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return airtaudio::error_warning;
	}
	m_state = airtaudio::state_stopped;
	std11::unique_lock<std11::mutex> lck(m_mutex);
	int32_t result = 0;
	snd_pcm_t **handle = (snd_pcm_t **) m_private->handles;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		result = snd_pcm_drop(handle[0]);
		if (result < 0) {
			ATA_ERROR("error aborting output pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
	if (    (    m_mode == airtaudio::mode_input
	          || m_mode == airtaudio::mode_duplex)
	     && !m_private->synchronized) {
		result = snd_pcm_drop(handle[1]);
		if (result < 0) {
			ATA_ERROR("error aborting input pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
unlock:
	if (result >= 0) {
		return airtaudio::error_none;
	}
	return airtaudio::error_systemError;
}


void airtaudio::api::Alsa::alsaCallbackEvent(void *_userData) {
	airtaudio::api::Alsa* myClass = reinterpret_cast<airtaudio::api::Alsa*>(_userData);
	myClass->callbackEvent();
}

void airtaudio::api::Alsa::callbackEvent() {
	etk::log::setThreadName("Alsa IO-" + m_name);
	while (m_private->threadRunning == true) {
		callbackEventOneCycle();
	}
}

namespace std {
	static std::ostream& operator <<(std::ostream& _os, const std11::chrono::system_clock::time_point& _obj) {
		#if __CPP_VERSION__ >= 2011
			std11::chrono::nanoseconds ns = std11::chrono::duration_cast<std11::chrono::nanoseconds>(_obj.time_since_epoch());
		#else
			boost::chrono::nanoseconds ns = boost::chrono::duration_cast<boost::chrono::nanoseconds>(_obj.time_since_epoch());
		#endif
		int64_t totalSecond = ns.count()/1000000000;
		int64_t millisecond = (ns.count()%1000000000)/1000000;
		int64_t microsecond = (ns.count()%1000000)/1000;
		int64_t nanosecond = ns.count()%1000;
		//_os << totalSecond << "s " << millisecond << "ms " << microsecond << "µs " << nanosecond << "ns";
		
		int32_t second = totalSecond % 60;
		int32_t minute = (totalSecond/60)%60;
		int32_t hour = (totalSecond/3600)%24;
		int32_t day = (totalSecond/(24*3600))%365;
		int32_t year = totalSecond/(24*3600*365);
		_os << year << "y " << day << "d " << hour << "h" << minute << ":"<< second << "s " << millisecond << "ms " << microsecond << "Âµs " << nanosecond << "ns";
		return _os;
	}
}
/*
std11::chrono::time_point<std11::chrono::system_clock> airtaudio::api::Alsa::getStreamTime() {
	if (m_startTime == std11::chrono::system_clock::time_point()) {
		snd_pcm_uframes_t avail;
		snd_htimestamp_t tstamp;
		if (m_private->handles[0] != nullptr) {
			int plop = snd_pcm_htimestamp(m_private->handles[0], &avail, &tstamp);
		} else if (m_private->handles[1] != nullptr) {
			int plop = snd_pcm_htimestamp(m_private->handles[1], &avail, &tstamp);
		}
		//ATA_WARNING("plop : " << tstamp.tv_sec << " sec " << tstamp.tv_nsec);
		//return std11::chrono::system_clock::from_time_t(tstamp.tv_sec) + std11::chrono::nanoseconds(tstamp.tv_nsec);
		m_startTime = std11::chrono::system_clock::from_time_t(tstamp.tv_sec) + std11::chrono::nanoseconds(tstamp.tv_nsec);
		m_startTime = std11::chrono::system_clock::now();
		if (m_private->handles[0] != nullptr) {
			//m_startTime += std11::chrono::nanoseconds(m_bufferSize*1000000000LL/int64_t(m_sampleRate));
			snd_pcm_sframes_t frames;
			int result = snd_pcm_delay(m_private->handles[0], &frames);
			m_startTime += std11::chrono::nanoseconds(frames*1000000000LL/int64_t(m_sampleRate));
		} else if (m_private->handles[1] != nullptr) {
			//m_startTime -= std11::chrono::nanoseconds(m_bufferSize*1000000000LL/int64_t(m_sampleRate));
			snd_pcm_sframes_t frames;
			int result = snd_pcm_delay(m_private->handles[1], &frames);
			m_startTime -= std11::chrono::nanoseconds(frames*1000000000LL/int64_t(m_sampleRate));
		}
		
		m_duration = std11::chrono::microseconds(0);
	}
	//ATA_DEBUG(" createTimeStamp : " << m_startTime + m_duration);
	
	return m_startTime + m_duration;
}
*/
#if 1

std11::chrono::system_clock::time_point airtaudio::api::Alsa::getStreamTime() {
	if (m_startTime == std11::chrono::system_clock::time_point()) {
		m_startTime = std11::chrono::system_clock::now();
		// Corection of time stamp with the input properties ...
		if (m_private->handles[0] != nullptr) {
			//output
			snd_pcm_sframes_t frames;
			int result = snd_pcm_delay(m_private->handles[0], &frames);
			m_startTime += std11::chrono::nanoseconds(frames*1000000000LL/int64_t(m_sampleRate));
		} else if (m_private->handles[1] != nullptr) {
			//input
			/*
			snd_pcm_sframes_t frames;
			int result = snd_pcm_delay(m_private->handles[1], &frames);
			m_startTime -= std11::chrono::nanoseconds(frames*1000000000LL/int64_t(m_sampleRate));
			*/
		}
		m_duration = std11::chrono::microseconds(0);
	}
	//ATA_DEBUG(" createTimeStamp : " << m_startTime + m_duration);
	
	return m_startTime + m_duration;
}
#else
std11::chrono::time_point<std11::chrono::system_clock> airtaudio::api::Alsa::getStreamTime() {
	snd_pcm_status_t *status = nullptr;
	snd_timestamp_t timestamp;
	snd_htimestamp_t timestampHighRes;
	snd_pcm_status_alloca(&status);
	if(m_private->handles[1]) {
		snd_pcm_status(m_private->handles[1], status);
		snd_pcm_status_get_tstamp(status, &timestamp);
		m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::microseconds(timestamp.tv_usec);
		ATA_WARNING("time : " << m_startTime);
		snd_pcm_status_get_trigger_tstamp(status, &timestamp);
		m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::microseconds(timestamp.tv_usec);
		ATA_WARNING("snd_pcm_status_get_trigger_tstamp : " << m_startTime);
		snd_pcm_status_get_htstamp(status, &timestampHighRes);
		ATA_WARNING("snd_pcm_status_get_htstamp : " << std11::chrono::system_clock::from_time_t(timestampHighRes.tv_sec) + std11::chrono::nanoseconds(timestampHighRes.tv_nsec););
		snd_pcm_status_get_audio_htstamp(status, &timestampHighRes);
		ATA_WARNING("snd_pcm_status_get_audio_htstamp : " << std11::chrono::system_clock::from_time_t(timestampHighRes.tv_sec) + std11::chrono::nanoseconds(timestampHighRes.tv_nsec););
		
		snd_pcm_sframes_t delay = snd_pcm_status_get_delay(status);
		//return m_startTime - std11::chrono::nanoseconds(delay*1000000000LL/int64_t(m_sampleRate));
		ATA_WARNING("delay : " << std11::chrono::nanoseconds(delay*1000000000LL/int64_t(m_sampleRate)).count() << " ns");
		return m_startTime + m_duration;
	}
	if(m_private->handles[0]) {
		snd_pcm_status(m_private->handles[0], status);
		snd_pcm_status_get_tstamp(status, &timestamp);
		m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::microseconds(timestamp.tv_usec);
		ATA_WARNING("time : " << m_startTime);
		snd_pcm_status_get_trigger_tstamp(status, &timestamp);
		m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::microseconds(timestamp.tv_usec);
		ATA_WARNING("start : " << m_startTime);
		snd_pcm_sframes_t delay = snd_pcm_status_get_delay(status);
		//return m_startTime + std11::chrono::nanoseconds(delay*1000000000LL/int64_t(m_sampleRate));
		ATA_WARNING("delay : " << std11::chrono::nanoseconds(delay*1000000000LL/int64_t(m_sampleRate)).count() << " ns");
		return m_startTime + m_duration;
	}
	return std11::chrono::system_clock::now();
}
#endif

void airtaudio::api::Alsa::callbackEventOneCycle() {
	if (m_state == airtaudio::state_stopped) {
		std11::unique_lock<std11::mutex> lck(m_mutex);
		// TODO : Set this back ....
		/*
		while (!m_private->runnable) {
			m_private->runnable_cv.wait(lck);
		}
		*/
		if (m_state != airtaudio::state_running) {
			return;
		}
	}
	if (m_state == airtaudio::state_closed) {
		ATA_CRITICAL("the stream is closed ... this shouldn't happen!");
		return; // TODO : notify appl: airtaudio::error_warning;
	}
	int32_t doStopStream = 0;
	std11::chrono::system_clock::time_point streamTime;
	std::vector<enum airtaudio::status> status;
	if (    m_mode != airtaudio::mode_input
	     && m_private->xrun[0] == true) {
		status.push_back(airtaudio::status_underflow);
		m_private->xrun[0] = false;
	}
	if (    m_mode != airtaudio::mode_output
	     && m_private->xrun[1] == true) {
		status.push_back(airtaudio::status_overflow);
		m_private->xrun[1] = false;
	}
	int32_t result;
	char *buffer;
	int32_t channels;
	snd_pcm_t **handle;
	snd_pcm_sframes_t frames;
	audio::format format;
	handle = (snd_pcm_t **) m_private->handles;
	
	if (m_state == airtaudio::state_stopped) {
		goto unlock;
	}
	
	if (    m_mode == airtaudio::mode_input
	     || m_mode == airtaudio::mode_duplex) {
		std11::unique_lock<std11::mutex> lck(m_mutex);
		// Setup parameters.
		if (m_doConvertBuffer[1]) {
			buffer = m_deviceBuffer;
			channels = m_nDeviceChannels[1];
			format = m_deviceFormat[1];
		} else {
			buffer = &m_userBuffer[1][0];
			channels = m_nUserChannels[1];
			format = m_userFormat;
		}
		// Read samples from device in interleaved/non-interleaved format.
		if (m_deviceInterleaved[1]) {
			result = snd_pcm_readi(handle[1], buffer, m_bufferSize);
		} else {
			void *bufs[channels];
			size_t offset = m_bufferSize * audio::getFormatBytes(format);
			for (int32_t i=0; i<channels; i++)
				bufs[i] = (void *) (buffer + (i * offset));
			result = snd_pcm_readn(handle[1], bufs, m_bufferSize);
		}
		// get timestamp : (to init here ...
		streamTime = getStreamTime();
		if (result < (int) m_bufferSize) {
			// Either an error or overrun occured.
			if (result == -EPIPE) {
				snd_pcm_state_t state = snd_pcm_state(handle[1]);
				if (state == SND_PCM_STATE_XRUN) {
					m_private->xrun[1] = true;
					result = snd_pcm_prepare(handle[1]);
					if (result < 0) {
						ATA_ERROR("error preparing device after overrun, " << snd_strerror(result) << ".");
					}
				} else {
					ATA_ERROR("error, current state is " << snd_pcm_state_name(state) << ", " << snd_strerror(result) << ".");
				}
			} else {
				ATA_ERROR("audio read error, " << snd_strerror(result) << ".");
			}
			// TODO : Notify application ... airtaudio::error_warning;
			goto noInput;
		}
		// Do byte swapping if necessary.
		if (m_doByteSwap[1]) {
			byteSwapBuffer(buffer, m_bufferSize * channels, format);
		}
		// Do buffer conversion if necessary.
		if (m_doConvertBuffer[1]) {
			convertBuffer(&m_userBuffer[1][0], m_deviceBuffer, m_convertInfo[1]);
		}
		// Check stream latency
		result = snd_pcm_delay(handle[1], &frames);
		if (result == 0 && frames > 0) {
			ATA_WARNING("Delay in the Input " << frames << " chunk");
			m_latency[1] = frames;
		}
	}

noInput:
	streamTime = getStreamTime();
	doStopStream = m_callback(&m_userBuffer[1][0],
	                          streamTime,// - std11::chrono::nanoseconds(m_latency[1]*1000000000LL/int64_t(m_sampleRate)),
	                          &m_userBuffer[0][0],
	                          streamTime,// + std11::chrono::nanoseconds(m_latency[0]*1000000000LL/int64_t(m_sampleRate)),
	                          m_bufferSize,
	                          status);
	if (doStopStream == 2) {
		abortStream();
		return;
	}

	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		std11::unique_lock<std11::mutex> lck(m_mutex);
		// Setup parameters and do buffer conversion if necessary.
		if (m_doConvertBuffer[0]) {
			buffer = m_deviceBuffer;
			convertBuffer(buffer, &m_userBuffer[0][0], m_convertInfo[0]);
			channels = m_nDeviceChannels[0];
			format = m_deviceFormat[0];
		} else {
			buffer = &m_userBuffer[0][0];
			channels = m_nUserChannels[0];
			format = m_userFormat;
		}
		// Do byte swapping if necessary.
		if (m_doByteSwap[0]) {
			byteSwapBuffer(buffer, m_bufferSize * channels, format);
		}
		// Write samples to device in interleaved/non-interleaved format.
		if (m_deviceInterleaved[0]) {
			result = snd_pcm_writei(handle[0], buffer, m_bufferSize);
		} else {
			void *bufs[channels];
			size_t offset = m_bufferSize * audio::getFormatBytes(format);
			for (int32_t i=0; i<channels; i++) {
				bufs[i] = (void *) (buffer + (i * offset));
			}
			result = snd_pcm_writen(handle[0], bufs, m_bufferSize);
		}
		if (result < (int) m_bufferSize) {
			// Either an error or underrun occured.
			if (result == -EPIPE) {
				snd_pcm_state_t state = snd_pcm_state(handle[0]);
				if (state == SND_PCM_STATE_XRUN) {
					m_private->xrun[0] = true;
					result = snd_pcm_prepare(handle[0]);
					if (result < 0) {
						ATA_ERROR("error preparing device after underrun, " << snd_strerror(result) << ".");
					}
				} else {
					ATA_ERROR("error, current state is " << snd_pcm_state_name(state) << ", " << snd_strerror(result) << ".");
				}
			} else {
				ATA_ERROR("audio write error, " << snd_strerror(result) << ".");
			}
			// TODO : Notuify application airtaudio::error_warning;
			goto unlock;
		}
		// Check stream latency
		result = snd_pcm_delay(handle[0], &frames);
		if (result == 0 && frames > 0) {
			ATA_WARNING("Delay in the Output " << frames << " chunk");
			m_latency[0] = frames;
		}
	}

unlock:
	airtaudio::Api::tickStreamTime();
	if (doStopStream == 1) {
		this->stopStream();
	}
}


#endif
