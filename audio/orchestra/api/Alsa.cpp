/** @file
 * @author Edouard DUPIN
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */


#if defined(ORCHESTRA_BUILD_ALSA)

#include <alsa/asoundlib.h>
#include <unistd.h>
#include <audio/orchestra/Interface.h>
#include <audio/orchestra/debug.h>
#include <etk/stdTools.h>
#include <etk/thread/tools.h>
#include <limits.h>
#include <audio/orchestra/api/Alsa.h>

#undef __class__
#define __class__ "api::Alsa"

audio::orchestra::Api* audio::orchestra::api::Alsa::Create() {
	return new audio::orchestra::api::Alsa();
}

namespace audio {
	namespace orchestra {
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
					enum timestampMode timeMode; //!< the timestamp of the flow came from the harware.
					AlsaPrivate() :
					  synchronized(false),
					  runnable(false),
					  thread(nullptr),
					  threadRunning(false),
					  timeMode(timestampMode_soft) {
						handles[0] = nullptr;
						handles[1] = nullptr;
						xrun[0] = false;
						xrun[1] = false;
						// TODO : Wait thread ...
					}
			};
		}
	}
}

audio::orchestra::api::Alsa::Alsa() :
  m_private(new audio::orchestra::api::AlsaPrivate()) {
	// Nothing to do here.
}

audio::orchestra::api::Alsa::~Alsa() {
	if (m_state != audio::orchestra::state_closed) {
		closeStream();
	}
}

uint32_t audio::orchestra::api::Alsa::getDeviceCount() {
	unsigned nDevices = 0;
	int32_t result= -1;
	int32_t subdevice = -1;
	int32_t card = -1;
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
			// TODO : Return error audio::orchestra::error_warning;
			goto nextcard;
		}
		subdevice = -1;
		while(1) {
			result = snd_ctl_pcm_next_device(handle, &subdevice);
			if (result < 0) {
				ATA_ERROR("control next device, card = " << card << ", " << snd_strerror(result) << ".");
				// TODO : Return error audio::orchestra::error_warning;
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
	return nDevices;
}

bool audio::orchestra::api::Alsa::getNamedDeviceInfoLocal(const std::string& _deviceName, audio::orchestra::DeviceInfo& _info, int32_t _cardId, int32_t _subdevice, int32_t _localDeviceId) {
	int32_t result;
	snd_ctl_t *chandle;
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
	std::vector<std::string> listElement = etk::split(_deviceName, ',');
	if (listElement.size() == 0) {
		ATA_ERROR("can not get control interface = '" << _deviceName << "' Can not plit at ',' ...");
		return false;
	}
	if (listElement.size() == 1) {
		// need to check if it is an input or output:
		listElement = etk::split(_deviceName, '_');
	}
	ATA_INFO("Open control : " << listElement[0]);
	result = snd_ctl_open(&chandle, listElement[0].c_str(), SND_CTL_NONBLOCK);
	if (result < 0) {
		ATA_ERROR("control open, card = " << listElement[0] << ", " << snd_strerror(result) << ".");
		return false;
	}
	if (_subdevice != -1) {
		snd_pcm_info_set_device(pcminfo, _subdevice);
		snd_pcm_info_set_subdevice(pcminfo, 0);
		result = snd_ctl_pcm_info(chandle, pcminfo);
		if (result < 0) {
			// Device probably doesn't support playback.
			goto captureProbe;
		}
	}
	result = snd_pcm_open(&phandle, _deviceName.c_str(), stream, openMode | SND_PCM_NONBLOCK);
	if (result < 0) {
		ATA_ERROR("snd_pcm_open error for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		goto captureProbe;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("snd_pcm_hw_params error for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		goto captureProbe;
	}
	// Get output channel information.
	uint32_t value;
	result = snd_pcm_hw_params_get_channels_max(params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting device (" << _deviceName << ") output channels, " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		goto captureProbe;
	}
	_info.outputChannels = value;
	snd_pcm_close(phandle);

captureProbe:
	stream = SND_PCM_STREAM_CAPTURE;
	snd_pcm_info_set_stream(pcminfo, stream);
	// Now try for capture unless default device (with subdev = -1)
	if (_subdevice != -1) {
		result = snd_ctl_pcm_info(chandle, pcminfo);
		snd_ctl_close(chandle);
		if (result < 0) {
			// Device probably doesn't support capture.
			if (_info.outputChannels == 0) {
				return true;
			}
			goto probeParameters;
		}
	}
	result = snd_pcm_open(&phandle, _deviceName.c_str(), stream, openMode | SND_PCM_NONBLOCK);
	if (result < 0) {
		ATA_ERROR("snd_pcm_open error for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		if (_info.outputChannels == 0) {
			return true;
		}
		goto probeParameters;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("snd_pcm_hw_params error for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		if (_info.outputChannels == 0) {
			return true;
		}
		goto probeParameters;
	}
	result = snd_pcm_hw_params_get_channels_max(params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting device (" << _deviceName << ") input channels, " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		if (_info.outputChannels == 0) {
			return true;
		}
		goto probeParameters;
	}
	_info.inputChannels = value;
	snd_pcm_close(phandle);
	// If device opens for both playback and capture, we determine the channels.
	if (    _info.outputChannels > 0
	     && _info.inputChannels > 0) {
		_info.duplexChannels = (_info.outputChannels > _info.inputChannels) ? _info.inputChannels : _info.outputChannels;
	}
	// ALSA doesn't provide default devices so we'll use the first available one.
	if (    _localDeviceId == 0
	     && _info.outputChannels > 0) {
		_info.isDefaultOutput = true;
	}
	if (    _localDeviceId == 0
	     && _info.inputChannels > 0) {
		_info.isDefaultInput = true;
	}

probeParameters:
	// At this point, we just need to figure out the supported data
	// formats and sample rates. We'll proceed by opening the device in
	// the direction with the maximum number of channels, or playback if
	// they are equal.	This might limit our sample rate options, but so
	// be it.
	if (_info.outputChannels >= _info.inputChannels) {
		stream = SND_PCM_STREAM_PLAYBACK;
	} else {
		stream = SND_PCM_STREAM_CAPTURE;
	}
	snd_pcm_info_set_stream(pcminfo, stream);
	result = snd_pcm_open(&phandle, _deviceName.c_str(), stream, openMode | SND_PCM_NONBLOCK);
	if (result < 0) {
		ATA_ERROR("snd_pcm_open error for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		return false;
	}
	// The device is open ... fill the parameter structure.
	result = snd_pcm_hw_params_any(phandle, params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("snd_pcm_hw_params error for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		// TODO : Return audio::orchestra::error_warning;
		return false;
	}
	// Test our discrete set of sample rate values.
	_info.sampleRates.clear();
	for (std::vector<uint32_t>::const_iterator it(audio::orchestra::genericSampleRate().begin()); 
	     it != audio::orchestra::genericSampleRate().end();
	     ++it ) {
		if (snd_pcm_hw_params_test_rate(phandle, params, *it, 0) == 0) {
			_info.sampleRates.push_back(*it);
		}
	}
	if (_info.sampleRates.size() == 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("no supported sample rates found for device (" << _deviceName << ").");
		// TODO : Return audio::orchestra::error_warning;
		return false;
	}
	// Probe the supported data formats ... we don't care about endian-ness just yet
	snd_pcm_format_t format;
	_info.nativeFormats.clear();
	format = SND_PCM_FORMAT_S8;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		_info.nativeFormats.push_back(audio::format_int8);
	}
	format = SND_PCM_FORMAT_S16;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		_info.nativeFormats.push_back(audio::format_int16);
	}
	format = SND_PCM_FORMAT_S24;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		_info.nativeFormats.push_back(audio::format_int24);
	}
	format = SND_PCM_FORMAT_S32;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		_info.nativeFormats.push_back(audio::format_int32);
	}
	format = SND_PCM_FORMAT_FLOAT;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		_info.nativeFormats.push_back(audio::format_float);
	}
	format = SND_PCM_FORMAT_FLOAT64;
	if (snd_pcm_hw_params_test_format(phandle, params, format) == 0) {
		_info.nativeFormats.push_back(audio::format_double);
	}
	// Check that we have at least one supported format
	if (_info.nativeFormats.size() == 0) {
		ATA_ERROR("pcm device (" << _deviceName << ") data format not supported by RtAudio.");
		// TODO : Return audio::orchestra::error_warning;
		return false;
	}
	// Get the device name
	if (_cardId != -1) {
		char *cardname;
		char name[1024];
		result = snd_card_get_name(_cardId, &cardname);
		if (result >= 0) {
			sprintf(name, "hw:%s,%d", cardname, _subdevice);
		}
		_info.name = name;
	} else {
		_info.name = _deviceName;
	}
	// That's all ... close the device and return
	snd_pcm_close(phandle);
	_info.probed = true;
	return true;
}

audio::orchestra::DeviceInfo audio::orchestra::api::Alsa::getDeviceInfo(uint32_t _device) {
	audio::orchestra::DeviceInfo info;
	/*
	ATA_WARNING("plop");
	getDeviceInfo("hw:0,0,0", info);
	info.display();
	getDeviceInfo("hw:0,0,1", info);
	info.display();
	*/
	info.probed = false;
	unsigned nDevices = 0;
	int32_t result = -1;
	int32_t subdevice = -1;
	int32_t card = -1;
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
	if (nDevices == 0) {
		ATA_ERROR("no devices found!");
		// TODO : audio::orchestra::error_invalidUse;
		return info;
	}
	if (_device >= nDevices) {
		ATA_ERROR("device ID is invalid!");
		// TODO : audio::orchestra::error_invalidUse;
		return info;
	}

foundDevice:
	snd_ctl_close(chandle);
	// If a stream is already open, we cannot probe the stream devices.
	// Thus, use the saved results.
	if (    m_state != audio::orchestra::state_closed
	     && (    m_device[0] == _device
	          || m_device[1] == _device)) {
		if (_device >= m_devices.size()) {
			ATA_ERROR("device ID was not present before stream was opened.");
			// TODO : return audio::orchestra::error_warning;
			return info;
		}
		return m_devices[ _device ];
	}
	bool ret = audio::orchestra::api::Alsa::getNamedDeviceInfoLocal(name, info, card, subdevice, _device);
	if (ret == false) {
		// TODO : ...
		return info;
	}
	return info;
}

void audio::orchestra::api::Alsa::saveDeviceInfo() {
	m_devices.clear();
	uint32_t nDevices = getDeviceCount();
	m_devices.resize(nDevices);
	for (uint32_t iii=0; iii<nDevices; ++iii) {
		m_devices[iii] = getDeviceInfo(iii);
	}
}

bool audio::orchestra::api::Alsa::probeDeviceOpen(uint32_t _device,
                                           audio::orchestra::mode _mode,
                                           uint32_t _channels,
                                           uint32_t _firstChannel,
                                           uint32_t _sampleRate,
                                           audio::format _format,
                                           uint32_t *_bufferSize,
                                           const audio::orchestra::StreamOptions& _options) {
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
	return probeDeviceOpenName(name, _mode, _channels, _firstChannel, _sampleRate, _format, _bufferSize, _options);
}

bool audio::orchestra::api::Alsa::probeDeviceOpenName(const std::string& _deviceName,
                                               audio::orchestra::mode _mode,
                                               uint32_t _channels,
                                               uint32_t _firstChannel,
                                               uint32_t _sampleRate,
                                               audio::format _format,
                                               uint32_t *_bufferSize,
                                               const audio::orchestra::StreamOptions& _options) {
	ATA_DEBUG("Probe ALSA device : ");
	ATA_DEBUG("    _deviceName=" << _deviceName);
	ATA_DEBUG("    _mode=" << _mode);
	ATA_DEBUG("    _channels=" << _channels);
	ATA_DEBUG("    _firstChannel=" << _firstChannel);
	ATA_DEBUG("    _sampleRate=" << _sampleRate);
	ATA_DEBUG("    _format=" << _format);
	
	
	// I'm not using the "plug" interface ... too much inconsistent behavior.
	unsigned nDevices = 0;
	int32_t result, subdevice, card;
	char name[64];
	snd_ctl_t *chandle;
	
	// The getDeviceInfo() function will not work for a device that is
	// already open.	Thus, we'll probe the system before opening a
	// stream and save the results for use by getDeviceInfo().
	if (    _mode == audio::orchestra::mode_output
	    || (    _mode == audio::orchestra::mode_input
	         && m_mode != audio::orchestra::mode_output)) {
		// only do once
		this->saveDeviceInfo();
	}
	snd_pcm_stream_t stream;
	if (_mode == audio::orchestra::mode_output) {
		stream = SND_PCM_STREAM_PLAYBACK;
	} else {
		stream = SND_PCM_STREAM_CAPTURE;
	}
	snd_pcm_t *phandle;
	int32_t openMode = SND_PCM_ASYNC;
	result = snd_pcm_open(&phandle, _deviceName.c_str(), stream, openMode);
	ATA_DEBUG("Configure Mode : SND_PCM_ASYNC");
	if (result < 0) {
		if (_mode == audio::orchestra::mode_output) {
			ATA_ERROR("pcm device (" << _deviceName << ") won't open for output.");
		} else {
			ATA_ERROR("pcm device (" << _deviceName << ") won't open for input.");
		}
		return false;
	}
	// Fill the parameter structure.
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_hw_params_alloca(&hw_params);
	result = snd_pcm_hw_params_any(phandle, hw_params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting pcm device (" << _deviceName << ") parameters, " << snd_strerror(result) << ".");
		return false;
	}
	// Open stream all time in interleave mode (by default): (open in non interleave if we have no choice
	ATA_DEBUG("configure Acces: SND_PCM_ACCESS_RW_INTERLEAVED");
	result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (result < 0) {
		ATA_DEBUG("configure Acces: SND_PCM_ACCESS_RW_NONINTERLEAVED");
		result = snd_pcm_hw_params_set_access(phandle, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
		m_deviceInterleaved[modeToIdTable(_mode)] =	false;
	} else {
		m_deviceInterleaved[modeToIdTable(_mode)] =	true;
	}
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting pcm device (" << _deviceName << ") access, " << snd_strerror(result) << ".");
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
		ATA_ERROR("pcm device " << _deviceName << " data format not supported: " << _format);
		// TODO : display list of all supported format ..
		return false;
	}
	ATA_DEBUG("configure format: " << _format);
	result = snd_pcm_hw_params_set_format(phandle, hw_params, deviceFormat);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting pcm device (" << _deviceName << ") data format, " << snd_strerror(result) << ".");
		return false;
	}
	// Determine whether byte-swaping is necessary.
	m_doByteSwap[modeToIdTable(_mode)] = false;
	if (deviceFormat != SND_PCM_FORMAT_S8) {
		result = snd_pcm_format_cpu_endian(deviceFormat);
		if (result == 0) {
			ATA_DEBUG("configure swap Byte");
			m_doByteSwap[modeToIdTable(_mode)] = true;
		} else if (result < 0) {
			snd_pcm_close(phandle);
			ATA_ERROR("error getting pcm device (" << _deviceName << ") endian-ness, " << snd_strerror(result) << ".");
			return false;
		}
	}
	ATA_DEBUG("Set frequency " << _sampleRate);
	// Set the sample rate.
	result = snd_pcm_hw_params_set_rate_near(phandle, hw_params, (uint32_t*) &_sampleRate, 0);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting sample rate on device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	// Determine the number of channels for this device.	We support a possible
	// minimum device channel number > than the value requested by the user.
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	uint32_t value;
	result = snd_pcm_hw_params_get_channels_max(hw_params, &value);
	uint32_t deviceChannels = value;
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("requested channel parameters not supported by device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	if (deviceChannels < _channels + _firstChannel) {
		snd_pcm_close(phandle);
		ATA_ERROR("requested channel " << _channels << " have : " << deviceChannels );
		return false;
	}
	result = snd_pcm_hw_params_get_channels_min(hw_params, &value);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error getting minimum channels for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	deviceChannels = value;
	ATA_DEBUG("Device Channel : " << deviceChannels);
	if (deviceChannels < _channels + _firstChannel) {
		deviceChannels = _channels + _firstChannel;
	}
	ATA_DEBUG("snd_pcm_hw_params_set_channels: " << deviceChannels);
	m_nDeviceChannels[modeToIdTable(_mode)] = deviceChannels;
	// Set the device channels.
	result = snd_pcm_hw_params_set_channels(phandle, hw_params, deviceChannels);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting channels for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	ATA_DEBUG("configure channels : " << deviceChannels);
	// Set the buffer (or period) size.
	int32_t dir = 0;
	snd_pcm_uframes_t periodSize = *_bufferSize;
	result = snd_pcm_hw_params_set_period_size_near(phandle, hw_params, &periodSize, &dir);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting period size for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	*_bufferSize = periodSize;
	ATA_DEBUG("configure periode size :" << periodSize);

	// Set the buffer number, which in ALSA is referred to as the "period".
	uint32_t periods = 0;
	if (_options.flags.m_minimizeLatency == true) {
		periods = 2;
	}
	/* TODO : Chouse the number of low level buffer ...
	if (_options.numberOfBuffers > 0) {
		periods = _options.numberOfBuffers;
	}
	*/
	if (periods < 2) {
		periods = 4; // a fairly safe default value
	}
	result = snd_pcm_hw_params_set_periods_near(phandle, hw_params, &periods, &dir);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error setting periods for device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	ATA_DEBUG("configure Buffer number: " << periods);
	m_sampleRate = _sampleRate;
	// If attempting to setup a duplex stream, the bufferSize parameter
	// MUST be the same in both directions!
	if (    m_mode == audio::orchestra::mode_output
	     && _mode == audio::orchestra::mode_input
	     && *_bufferSize != m_bufferSize) {
		snd_pcm_close(phandle);
		ATA_ERROR("system error setting buffer size for duplex stream on device (" << _deviceName << ").");
		return false;
	}
	m_bufferSize = *_bufferSize;
	ATA_INFO("configure buffer size = " << m_bufferSize*periods << " periode time in ms=" << int64_t(double(m_bufferSize)/double(m_sampleRate)*1000000000.0) << "ns");
	// check if the hardware provide hardware clock :
	if (snd_pcm_hw_params_is_monotonic(hw_params) == 0) {
		ATA_INFO("ALSA Audio timestamp is NOT monotonic (Generate with the start timestamp)");
		if (_options.mode == timestampMode_Hardware) {
			ATA_WARNING("Can not select Harware timeStamp ==> the IO is not monotonic ==> select ");
			m_private->timeMode = timestampMode_trigered;
		} else {
			m_private->timeMode = _options.mode;
		}
	} else {
		ATA_DEBUG("ALSA Audio timestamp is monotonic (can came from harware)");
		m_private->timeMode = _options.mode;
	}

	// Install the hardware configuration
	result = snd_pcm_hw_params(phandle, hw_params);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error installing hardware configuration on device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	snd_pcm_uframes_t val;
	// Set the software configuration to fill buffers with zeros and prevent device stopping on xruns.
	snd_pcm_sw_params_t *swParams = nullptr;
	snd_pcm_sw_params_alloca(&swParams);
	snd_pcm_sw_params_current(phandle, swParams);
	#if 0
		ATA_DEBUG("configure start_threshold: " << int64_t(*_bufferSize));
		snd_pcm_sw_params_set_start_threshold(phandle, swParams, *_bufferSize);
	#else
		ATA_DEBUG("configure start_threshold: " << int64_t(1));
		snd_pcm_sw_params_set_start_threshold(phandle, swParams, 1);
	#endif
	#if 0
		ATA_DEBUG("configure stop_threshold: " << ULONG_MAX);
		snd_pcm_sw_params_set_stop_threshold(phandle, swParams, ULONG_MAX);
	#else
		ATA_DEBUG("configure stop_threshold: " << m_bufferSize*periods);
		snd_pcm_sw_params_set_stop_threshold(phandle, swParams, m_bufferSize*periods);
	#endif
	//ATA_DEBUG("configure silence_threshold: " << 0);
	//snd_pcm_sw_params_set_silence_threshold(phandle, swParams, 0);
	// The following two settings were suggested by Theo Veenker
	#if 0
		snd_pcm_sw_params_set_avail_min(phandle, swParams, *_bufferSize*periods/2);
		snd_pcm_sw_params_get_avail_min(swParams, &val);
		ATA_DEBUG("configure set availlable min: " << *_bufferSize*periods/2 << " really set: " << val);
	#endif
	//int valInt;
	//snd_pcm_sw_params_get_period_event(swParams, &valInt);
        //ATA_DEBUG("configure get period_event: " << valInt);
	//snd_pcm_sw_params_set_xfer_align(phandle, swParams, 1);
	// here are two options for a fix
	//snd_pcm_sw_params_set_silence_size(phandle, swParams, ULONG_MAX);
	
	//snd_pcm_sw_params_set_tstamp_mode(phandle, swParams, SND_PCM_TSTAMP_ENABLE);
	ATA_DEBUG("configuration: ");

	//ATA_DEBUG("    start_mode: " << snd_pcm_start_mode_name(snd_pcm_sw_params_get_start_mode(swParams)));

	//ATA_DEBUG("    xrun_mode: " << snd_pcm_xrun_mode_name(snd_pcm_sw_params_get_xrun_mode(swParams)));
	snd_pcm_tstamp_t valTsMode;
	snd_pcm_sw_params_get_tstamp_mode(swParams, &valTsMode);
	ATA_DEBUG("    tstamp_mode: " << snd_pcm_tstamp_mode_name(valTsMode));
	
	//ATA_DEBUG("    period_step: " << swParams->period_step);

	//ATA_DEBUG("    sleep_min: " << swParams->sleep_min);
	snd_pcm_sw_params_get_avail_min(swParams, &val);
	ATA_DEBUG("    avail_min: " << val);
	snd_pcm_sw_params_get_xfer_align(swParams, &val);
	ATA_DEBUG("    xfer_align: " << val);
	
	snd_pcm_sw_params_get_silence_threshold(swParams, &val);
	ATA_DEBUG("    silence_threshold: " << val);
	snd_pcm_sw_params_get_silence_size(swParams, &val);
	ATA_DEBUG("    silence_size: " << val);
	snd_pcm_sw_params_get_boundary(swParams, &val);
	ATA_DEBUG("    boundary: " << val);
	result = snd_pcm_sw_params(phandle, swParams);
	if (result < 0) {
		snd_pcm_close(phandle);
		ATA_ERROR("error installing software configuration on device (" << _deviceName << "), " << snd_strerror(result) << ".");
		return false;
	}
	
	{
		snd_pcm_uframes_t _period_size = 0;
		snd_pcm_uframes_t _buffer_size = 0;
		snd_pcm_hw_params_get_period_size(hw_params, &_period_size, &dir);
		snd_pcm_hw_params_get_buffer_size(hw_params, &_buffer_size);
		ATA_DEBUG("ploooooo _period_size=" << _period_size << " _buffer_size=" << _buffer_size);
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
		if (_mode == audio::orchestra::mode_input) {
			if (    m_mode == audio::orchestra::mode_output
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
	m_nBuffers = periods;
	ATA_INFO("ALSA NB buffer = " << m_nBuffers);
	// TODO : m_device[modeToIdTable(_mode)] = _device;
	m_state = audio::orchestra::state_stopped;
	// Setup the buffer conversion information structure.
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		setConvertInfo(_mode, _firstChannel);
	}
	// Setup thread if necessary.
	if (    m_mode == audio::orchestra::mode_output
	     && _mode == audio::orchestra::mode_input) {
		// We had already set up an output stream.
		m_mode = audio::orchestra::mode_duplex;
		// Link the streams if possible.
		m_private->synchronized = false;
		if (snd_pcm_link(m_private->handles[0], m_private->handles[1]) == 0) {
			m_private->synchronized = true;
		} else {
			ATA_ERROR("unable to synchronize input and output devices.");
			// TODO : audio::orchestra::error_warning;
		}
	} else {
		m_mode = _mode;
		// Setup callback thread.
		m_private->threadRunning = true;
		m_private->thread = new std11::thread(&audio::orchestra::api::Alsa::alsaCallbackEvent, this);
		if (m_private->thread == nullptr) {
			m_private->threadRunning = false;
			ATA_ERROR("creating callback thread!");
			goto error;
		}
		etk::thread::setPriority(*m_private->thread, -6);
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
	m_state = audio::orchestra::state_closed;
	return false;
}

enum audio::orchestra::error audio::orchestra::api::Alsa::closeStream() {
	if (m_state == audio::orchestra::state_closed) {
		ATA_ERROR("no open stream to close!");
		return audio::orchestra::error_warning;
	}
	m_private->threadRunning = false;
	m_mutex.lock();
	if (m_state == audio::orchestra::state_stopped) {
		m_private->runnable = true;
		m_private->runnable_cv.notify_one();
	}
	m_mutex.unlock();
	if (m_private->thread != nullptr) {
		m_private->thread->join();
		m_private->thread = nullptr;
	}
	if (m_state == audio::orchestra::state_running) {
		m_state = audio::orchestra::state_stopped;
		if (    m_mode == audio::orchestra::mode_output
		     || m_mode == audio::orchestra::mode_duplex) {
			snd_pcm_drop(m_private->handles[0]);
		}
		if (    m_mode == audio::orchestra::mode_input
		     || m_mode == audio::orchestra::mode_duplex) {
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
	m_mode = audio::orchestra::mode_unknow;
	m_state = audio::orchestra::state_closed;
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Alsa::startStream() {
	// TODO : Check return ...
	//audio::orchestra::Api::startStream();
	// This method calls snd_pcm_prepare if the device isn't already in that state.
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state_running) {
		ATA_ERROR("the stream is already running!");
		return audio::orchestra::error_warning;
	}
	std11::unique_lock<std11::mutex> lck(m_mutex);
	int32_t result = 0;
	snd_pcm_state_t state;
	snd_pcm_t **handle = (snd_pcm_t **) m_private->handles;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
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
				ATA_ERROR("error preparing output pcm device: ERR=" << snd_strerror(result) << ".");
				goto unlock;
			}
		}
	}
	if (    (    m_mode == audio::orchestra::mode_input
	          || m_mode == audio::orchestra::mode_duplex)
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
				ATA_ERROR("error preparing input pcm device: ERR=" << snd_strerror(result) << ".");
				goto unlock;
			}
		}
	}
	m_state = audio::orchestra::state_running;
unlock:
	m_private->runnable = true;
	m_private->runnable_cv.notify_one();
	if (result >= 0) {
		return audio::orchestra::error_none;
	}
	return audio::orchestra::error_systemError;
}

enum audio::orchestra::error audio::orchestra::api::Alsa::stopStream() {
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_state = audio::orchestra::state_stopped;
	std11::unique_lock<std11::mutex> lck(m_mutex);
	int32_t result = 0;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		if (m_private->synchronized) {
			result = snd_pcm_drop( m_private->handles[0]);
		} else {
			result = snd_pcm_drain( m_private->handles[0]);
		}
		if (result < 0) {
			ATA_ERROR("error draining output pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
	if (    (    m_mode == audio::orchestra::mode_input
	          || m_mode == audio::orchestra::mode_duplex)
	     && !m_private->synchronized) {
		result = snd_pcm_drop( m_private->handles[1]);
		if (result < 0) {
			ATA_ERROR("error stopping input pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
unlock:
	if (result >= 0) {
		return audio::orchestra::error_none;
	}
	return audio::orchestra::error_systemError;
}

enum audio::orchestra::error audio::orchestra::api::Alsa::abortStream() {
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state_stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_state = audio::orchestra::state_stopped;
	std11::unique_lock<std11::mutex> lck(m_mutex);
	int32_t result = 0;
	snd_pcm_t **handle = (snd_pcm_t **) m_private->handles;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		result = snd_pcm_drop(handle[0]);
		if (result < 0) {
			ATA_ERROR("error aborting output pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
	if (    (    m_mode == audio::orchestra::mode_input
	          || m_mode == audio::orchestra::mode_duplex)
	     && !m_private->synchronized) {
		result = snd_pcm_drop(handle[1]);
		if (result < 0) {
			ATA_ERROR("error aborting input pcm device, " << snd_strerror(result) << ".");
			goto unlock;
		}
	}
unlock:
	if (result >= 0) {
		return audio::orchestra::error_none;
	}
	return audio::orchestra::error_systemError;
}


void audio::orchestra::api::Alsa::alsaCallbackEvent(void *_userData) {
	audio::orchestra::api::Alsa* myClass = reinterpret_cast<audio::orchestra::api::Alsa*>(_userData);
	myClass->callbackEvent();
}

void audio::orchestra::api::Alsa::callbackEvent() {
	etk::thread::setName("Alsa IO-" + m_name);
	while (m_private->threadRunning == true) {
		callbackEventOneCycle();
	}
}

std11::chrono::system_clock::time_point audio::orchestra::api::Alsa::getStreamTime() {
	//ATA_DEBUG("mode : " << m_private->timeMode);
	if (m_private->timeMode == timestampMode_Hardware) {
		snd_pcm_status_t *status = nullptr;
		snd_pcm_status_alloca(&status);
		// get harware timestamp all the time:
		if (m_private->handles[0] != nullptr) {
			snd_pcm_status(m_private->handles[0], status);
		} else if (m_private->handles[1] != nullptr) {
			snd_pcm_status(m_private->handles[1], status);
		} else {
			ATA_WARNING(" get time of the signal error ...");
			return m_startTime + m_duration;
		}
		#if 1
			snd_timestamp_t timestamp;
			snd_pcm_status_get_tstamp(status, &timestamp);
			m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::microseconds(timestamp.tv_usec);
		#else
			#if 1
				snd_htimestamp_t timestamp;
				snd_pcm_status_get_htstamp(status, &timestamp);
				m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::nanoseconds(timestamp.tv_nsec);
			#else
				snd_htimestamp_t timestamp;
				snd_pcm_status_get_audio_htstamp(status, &timestamp);
				m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::nanoseconds(timestamp.tv_nsec);
				return m_startTime;
			#endif
		#endif
		ATA_VERBOSE("snd_pcm_status_get_htstamp : " << m_startTime);
		snd_pcm_sframes_t delay = snd_pcm_status_get_delay(status);
		std11::chrono::nanoseconds timeDelay(delay*1000000000LL/int64_t(m_sampleRate));
		ATA_VERBOSE("delay : " << timeDelay.count() << " ns");
		//return m_startTime + m_duration;
		if (m_private->handles[0] != nullptr) {
			// output
			m_startTime += timeDelay;
		} else {
			// input
			m_startTime -= timeDelay;
		}
		return m_startTime;
	} else if (m_private->timeMode == timestampMode_trigered) {
		if (m_startTime == std11::chrono::system_clock::time_point()) {
			snd_pcm_status_t *status = nullptr;
			snd_pcm_status_alloca(&status);
			// get harware timestamp all the time:
			if (m_private->handles[0] != nullptr) {
				snd_pcm_status(m_private->handles[0], status);
			} else if (m_private->handles[1] != nullptr) {
				snd_pcm_status(m_private->handles[1], status);
			} else {
				ATA_WARNING(" get time of the signal error ...");
				return m_startTime + m_duration;
			}
			// get start time:
			snd_timestamp_t timestamp;
			snd_pcm_status_get_trigger_tstamp(status, &timestamp);
			m_startTime = std11::chrono::system_clock::from_time_t(timestamp.tv_sec) + std11::chrono::microseconds(timestamp.tv_usec);
			ATA_VERBOSE("snd_pcm_status_get_trigger_tstamp : " << m_startTime);
		}
		return m_startTime + m_duration;
	} else {
		// softaware mode ...
		if (m_startTime == std11::chrono::system_clock::time_point()) {
			m_startTime = std11::chrono::system_clock::now();
			ATA_ERROR("START TIOMESTAMP : " << m_startTime);
			std11::chrono::nanoseconds timeDelay(m_bufferSize*1000000000LL/int64_t(m_sampleRate));
			if (m_private->handles[0] != nullptr) {
				// output
				m_startTime += timeDelay;
			} else {
				// input
				m_startTime -= timeDelay;
			}
			m_duration = std11::chrono::microseconds(0);
		}
		return m_startTime + m_duration;
	}
	return m_startTime + m_duration;
}

void audio::orchestra::api::Alsa::callbackEventOneCycle() {
	if (m_state == audio::orchestra::state_stopped) {
		std11::unique_lock<std11::mutex> lck(m_mutex);
		// TODO : Set this back ....
		/*
		while (!m_private->runnable) {
			m_private->runnable_cv.wait(lck);
		}
		*/
		usleep(1000);
		if (m_state != audio::orchestra::state_running) {
			return;
		}
	}
	if (m_state == audio::orchestra::state_closed) {
		ATA_CRITICAL("the stream is closed ... this shouldn't happen!");
		return; // TODO : notify appl: audio::orchestra::error_warning;
	}
	int32_t doStopStream = 0;
	std11::chrono::system_clock::time_point streamTime;
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
	int32_t result;
	char *buffer;
	int32_t channels;
	snd_pcm_sframes_t frames;
	audio::format format;
	
	if (m_state == audio::orchestra::state_stopped) {
		goto unlock;
	}
	
	if (    m_mode == audio::orchestra::mode_input
	     || m_mode == audio::orchestra::mode_duplex) {
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
			result = snd_pcm_readi(m_private->handles[1], buffer, m_bufferSize);
		} else {
			void *bufs[channels];
			size_t offset = m_bufferSize * audio::getFormatBytes(format);
			for (int32_t i=0; i<channels; i++)
				bufs[i] = (void *) (buffer + (i * offset));
			result = snd_pcm_readn(m_private->handles[1], bufs, m_bufferSize);
		}
		{
			snd_pcm_state_t state = snd_pcm_state(m_private->handles[1]);
			ATA_VERBOSE("plop : " << state);
			if (state == SND_PCM_STATE_XRUN) {
				ATA_ERROR("Xrun...");
			}
		}
		// get timestamp : (to init here ...
		streamTime = getStreamTime();
		if (result < (int) m_bufferSize) {
			// Either an error or overrun occured.
			if (result == -EPIPE) {
				snd_pcm_state_t state = snd_pcm_state(m_private->handles[1]);
				if (state == SND_PCM_STATE_XRUN) {
					m_private->xrun[1] = true;
					result = snd_pcm_prepare(m_private->handles[1]);
					if (result < 0) {
						ATA_ERROR("error preparing device after overrun, " << snd_strerror(result) << ".");
					}
				} else {
					ATA_ERROR("error, current state is " << snd_pcm_state_name(state) << ", " << snd_strerror(result) << ".");
				}
			} else {
				ATA_ERROR("audio read error, " << snd_strerror(result) << ".");
				usleep(10000);
			}
			// TODO : Notify application ... audio::orchestra::error_warning;
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
		result = snd_pcm_delay(m_private->handles[1], &frames);
		if (result == 0 && frames > 0) {
			ATA_VERBOSE("Delay in the Input " << frames << " chunk");
			m_latency[1] = frames;
		}
	}

noInput:
	streamTime = getStreamTime();
	{
		std11::chrono::system_clock::time_point startCall = std11::chrono::system_clock::now();
		doStopStream = m_callback(&m_userBuffer[1][0],
		                          streamTime,// - std11::chrono::nanoseconds(m_latency[1]*1000000000LL/int64_t(m_sampleRate)),
		                          &m_userBuffer[0][0],
		                          streamTime,// + std11::chrono::nanoseconds(m_latency[0]*1000000000LL/int64_t(m_sampleRate)),
		                          m_bufferSize,
		                          status);
		std11::chrono::system_clock::time_point stopCall = std11::chrono::system_clock::now();
		std11::chrono::nanoseconds timeDelay(m_bufferSize*1000000000LL/int64_t(m_sampleRate));
		std11::chrono::nanoseconds timeProcess = stopCall - startCall;
		if (timeDelay <= timeProcess) {
			ATA_ERROR("SOFT XRUN ... : (bufferTime) " << timeDelay.count() << " < " << timeProcess.count() << " (process time) ns");
		}
	}
	if (doStopStream == 2) {
		abortStream();
		return;
	}

	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
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
			result = snd_pcm_writei(m_private->handles[0], buffer, m_bufferSize);
		} else {
			void *bufs[channels];
			size_t offset = m_bufferSize * audio::getFormatBytes(format);
			for (int32_t i=0; i<channels; i++) {
				bufs[i] = (void *) (buffer + (i * offset));
			}
			result = snd_pcm_writen(m_private->handles[0], bufs, m_bufferSize);
		}
		if (result < (int) m_bufferSize) {
			// Either an error or underrun occured.
			if (result == -EPIPE) {
				snd_pcm_state_t state = snd_pcm_state(m_private->handles[0]);
				if (state == SND_PCM_STATE_XRUN) {
					m_private->xrun[0] = true;
					result = snd_pcm_prepare(m_private->handles[0]);
					if (result < 0) {
						ATA_ERROR("error preparing device after underrun, " << snd_strerror(result) << ".");
					}
				} else {
					ATA_ERROR("error, current state is " << snd_pcm_state_name(state) << ", " << snd_strerror(result) << ".");
				}
			} else {
				ATA_ERROR("audio write error, " << snd_strerror(result) << ".");
			}
			// TODO : Notuify application audio::orchestra::error_warning;
			goto unlock;
		}
		// Check stream latency
		result = snd_pcm_delay(m_private->handles[0], &frames);
		if (result == 0 && frames > 0) {
			ATA_VERBOSE("Delay in the Output " << frames << " chunk");
			m_latency[0] = frames;
		}
	}

unlock:
	audio::orchestra::Api::tickStreamTime();
	if (doStopStream == 1) {
		this->stopStream();
	}
}

bool audio::orchestra::api::Alsa::isMasterOf(audio::orchestra::Api* _api) {
	audio::orchestra::api::Alsa* slave = dynamic_cast<audio::orchestra::api::Alsa*>(_api);
	if (slave == nullptr) {
		ATA_ERROR("NULL ptr API (not ALSA ...)");
		return false;
	}
	if (m_state == audio::orchestra::state_running) {
		ATA_ERROR("The MASTER stream is already running! ==> can not synchronize ...");
		return false;
	}
	if (slave->m_state == audio::orchestra::state_running) {
		ATA_ERROR("The SLAVE stream is already running! ==> can not synchronize ...");
		return false;
	}
	snd_pcm_t * master = nullptr;
	if (m_private->handles[0] != nullptr) {
		master = m_private->handles[0];
	}
	if (m_private->handles[1] != nullptr) {
		master = m_private->handles[1];
	}
	if (master == nullptr) {
		ATA_ERROR("No ALSA handles ...");
		return false;
	}
	ATA_INFO("   ==> plop");
	if (slave->m_private->handles[0] != nullptr) {
		if (snd_pcm_link(master, slave->m_private->handles[0]) != 0) {
			ATA_ERROR("Can not syncronize handle output");
		} else {
			ATA_INFO("   -------------------- LINK 0 --------------------");
		}
	}
	if (slave->m_private->handles[1] != nullptr) {
		if (snd_pcm_link(master, slave->m_private->handles[1]) != 0) {
			ATA_ERROR("Can not syncronize handle input");
		} else {
			ATA_INFO("   -------------------- LINK 1 --------------------");
		}
	}
	return true;
}


#endif
