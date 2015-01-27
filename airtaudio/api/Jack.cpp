/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */


#if defined(__UNIX_JACK__)
#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <string.h>

#undef __class__
#define __class__ "api::Jack"

airtaudio::Api* airtaudio::api::Jack::Create() {
	return new airtaudio::api::Jack();
}


// JACK is a low-latency audio server, originally written for the
// GNU/Linux operating system and now also ported to OS-X. It can
// connect a number of different applications to an audio device, as
// well as allowing them to share audio between themselves.
//
// When using JACK with RtAudio, "devices" refer to JACK clients that
// have ports connected to the server.	The JACK server is typically
// started in a terminal as follows:
//
// .jackd -d alsa -d hw:0
//
// or through an interface program such as qjackctl.	Many of the
// parameters normally set for a stream are fixed by the JACK server
// and can be specified when the JACK server is started.	In
// particular,
//
// .jackd -d alsa -d hw:0 -r 44100 -p 512 -n 4
//
// specifies a sample rate of 44100 Hz, a buffer size of 512 sample
// frames, and number of buffers = 4.	Once the server is running, it
// is not possible to override these values.	If the values are not
// specified in the command-line, the JACK server uses default values.
//
// The JACK server does not have to be running when an instance of
// RtApiJack is created, though the function getDeviceCount() will
// report 0 devices found until JACK has been started.	When no
// devices are available (i.e., the JACK server is not running), a
// stream cannot be opened.

#include <jack/jack.h>
#include <unistd.h>
#include <cstdio>

// A structure to hold various information related to the Jack API
// implementation.
struct JackHandle {
	jack_client_t *client;
	jack_port_t **ports[2];
	std::string deviceName[2];
	bool xrun[2];
	std::condition_variable condition;
	int32_t drainCounter; // Tracks callback counts when draining
	bool internalDrain; // Indicates if stop is initiated from callback or not.

	JackHandle() :
	  client(0),
	  drainCounter(0),
	  internalDrain(false) {
		ports[0] = 0;
		ports[1] = 0;
		xrun[0] = false;
		xrun[1] = false;
	}
};

airtaudio::api::Jack::Jack() {
	// Nothing to do here.
}

airtaudio::api::Jack::~Jack() {
	if (m_stream.state != STREAM_CLOSED) {
		closeStream();
	}
}

uint32_t airtaudio::api::Jack::getDeviceCount() {
	// See if we can become a jack client.
	jack_options_t options = (jack_options_t) (JackNoStartServer); //JackNullOption;
	jack_status_t *status = nullptr;
	jack_client_t *client = jack_client_open("RtApiJackCount", options, status);
	if (client == nullptr) {
		return 0;
	}
	const char **ports;
	std::string port, previousPort;
	uint32_t nChannels = 0, nDevices = 0;
	ports = jack_get_ports(client, nullptr, nullptr, 0);
	if (ports) {
		// Parse the port names up to the first colon (:).
		size_t iColon = 0;
		do {
			port = (char *) ports[ nChannels ];
			iColon = port.find(":");
			if (iColon != std::string::npos) {
				port = port.substr(0, iColon + 1);
				if (port != previousPort) {
					nDevices++;
					previousPort = port;
				}
			}
		} while (ports[++nChannels]);
		free(ports);
	}
	jack_client_close(client);
	return nDevices;
}

airtaudio::DeviceInfo airtaudio::api::Jack::getDeviceInfo(uint32_t _device) {
	airtaudio::DeviceInfo info;
	info.probed = false;
	jack_options_t options = (jack_options_t) (JackNoStartServer); //JackNullOption
	jack_status_t *status = nullptr;
	jack_client_t *client = jack_client_open("RtApiJackInfo", options, status);
	if (client == nullptr) {
		ATA_ERROR("airtaudio::api::Jack::getDeviceInfo: Jack server not found or connection error!");
		// TODO : airtaudio::errorWarning;
		return info;
	}
	const char **ports;
	std::string port, previousPort;
	uint32_t nPorts = 0, nDevices = 0;
	ports = jack_get_ports(client, nullptr, nullptr, 0);
	if (ports) {
		// Parse the port names up to the first colon (:).
		size_t iColon = 0;
		do {
			port = (char *) ports[ nPorts ];
			iColon = port.find(":");
			if (iColon != std::string::npos) {
				port = port.substr(0, iColon);
				if (port != previousPort) {
					if (nDevices == _device) {
						info.name = port;
					}
					nDevices++;
					previousPort = port;
				}
			}
		} while (ports[++nPorts]);
		free(ports);
	}
	if (_device >= nDevices) {
		jack_client_close(client);
		ATA_ERROR("airtaudio::api::Jack::getDeviceInfo: device ID is invalid!");
		// TODO : airtaudio::errorInvalidUse;
		return info;
	}
	// Get the current jack server sample rate.
	info.sampleRates.clear();
	info.sampleRates.push_back(jack_get_sample_rate(client));
	// Count the available ports containing the client name as device
	// channels.	Jack "input ports" equal RtAudio output channels.
	uint32_t nChannels = 0;
	ports = jack_get_ports(client, info.name.c_str(), nullptr, JackPortIsInput);
	if (ports) {
		while (ports[ nChannels ]) {
			nChannels++;
		}
		free(ports);
		info.outputChannels = nChannels;
	}
	// Jack "output ports" equal RtAudio input channels.
	nChannels = 0;
	ports = jack_get_ports(client, info.name.c_str(), nullptr, JackPortIsOutput);
	if (ports) {
		while (ports[ nChannels ]) {
			nChannels++;
		}
		free(ports);
		info.inputChannels = nChannels;
	}
	if (info.outputChannels == 0 && info.inputChannels == 0) {
		jack_client_close(client);
		ATA_ERROR("airtaudio::api::Jack::getDeviceInfo: error determining Jack input/output channels!");
		// TODO : airtaudio::errorWarning;
		return info;
	}
	// If device opens for both playback and capture, we determine the channels.
	if (info.outputChannels > 0 && info.inputChannels > 0) {
		info.duplexChannels = (info.outputChannels > info.inputChannels) ? info.inputChannels : info.outputChannels;
	}
	// Jack always uses 32-bit floats.
	info.nativeFormats = airtaudio::FLOAT32;
	// Jack doesn't provide default devices so we'll use the first available one.
	if (    _device == 0
	     && info.outputChannels > 0) {
		info.isDefaultOutput = true;
	}
	if (    _device == 0
	     && info.inputChannels > 0) {
		info.isDefaultInput = true;
	}
	jack_client_close(client);
	info.probed = true;
	return info;
}

static int32_t jackCallbackHandler(jack_nframes_t _nframes, void *_infoPointer) {
	airtaudio::CallbackInfo* info = (airtaudio::CallbackInfo*)_infoPointer;
	airtaudio::api::Jack* object = (airtaudio::api::Jack*)info->object;
	if (object->callbackEvent((uint64_t)_nframes) == false) {
		return 1;
	}
	return 0;
}

// This function will be called by a spawned thread when the Jack
// server signals that it is shutting down.	It is necessary to handle
// it this way because the jackShutdown() function must return before
// the jack_deactivate() function (in closeStream()) will return.
static void jackCloseStream(void *_ptr) {
	airtaudio::CallbackInfo* info = (airtaudio::CallbackInfo*)_ptr;
	airtaudio::api::Jack* object = (airtaudio::api::Jack*)info->object;
	object->closeStream();
}

static void jackShutdown(void* _infoPointer) {
	airtaudio::CallbackInfo* info = (airtaudio::CallbackInfo*)_infoPointer;
	airtaudio::api::Jack* object = (airtaudio::api::Jack*)info->object;
	// Check current stream state. If stopped, then we'll assume this
	// was called as a result of a call to airtaudio::api::Jack::stopStream (the
	// deactivation of a client handle causes this function to be called).
	// If not, we'll assume the Jack server is shutting down or some
	// other problem occurred and we should close the stream.
	if (object->isStreamRunning() == false) {
		return;
	}
	new std::thread(jackCloseStream, info);
	ATA_ERROR("RtApiJack: the Jack server is shutting down this client ... stream stopped and closed!!");
}

static int32_t jackXrun(void* _infoPointer) {
	JackHandle* handle = (JackHandle*)_infoPointer;
	if (handle->ports[0]) {
		handle->xrun[0] = true;
	}
	if (handle->ports[1]) {
		handle->xrun[1] = true;
	}
	return 0;
}

bool airtaudio::api::Jack::probeDeviceOpen(uint32_t _device,
                                           airtaudio::api::StreamMode _mode,
                                           uint32_t _channels,
                                           uint32_t _firstChannel,
                                           uint32_t _sampleRate,
                                           airtaudio::format _format,
                                           uint32_t* _bufferSize,
                                           airtaudio::StreamOptions* _options) {
	JackHandle *handle = (JackHandle *) m_stream.apiHandle;
	// Look for jack server and try to become a client (only do once per stream).
	jack_client_t *client = 0;
	if (    _mode == OUTPUT
	     || (    _mode == INPUT
	          && m_stream.mode != OUTPUT)) {
		jack_options_t jackoptions = (jack_options_t) (JackNoStartServer); //JackNullOption;
		jack_status_t *status = nullptr;
		if (_options && !_options->streamName.empty()) {
			client = jack_client_open(_options->streamName.c_str(), jackoptions, status);
		} else {
			client = jack_client_open("RtApiJack", jackoptions, status);
		}
		if (client == 0) {
			ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: Jack server not found or connection error!");
			return false;
		}
	} else {
		// The handle must have been created on an earlier pass.
		client = handle->client;
	}
	const char **ports;
	std::string port, previousPort, deviceName;
	uint32_t nPorts = 0, nDevices = 0;
	ports = jack_get_ports(client, nullptr, nullptr, 0);
	if (ports) {
		// Parse the port names up to the first colon (:).
		size_t iColon = 0;
		do {
			port = (char *) ports[ nPorts ];
			iColon = port.find(":");
			if (iColon != std::string::npos) {
				port = port.substr(0, iColon);
				if (port != previousPort) {
					if (nDevices == _device) {
						deviceName = port;
					}
					nDevices++;
					previousPort = port;
				}
			}
		} while (ports[++nPorts]);
		free(ports);
	}
	if (_device >= nDevices) {
		ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: device ID is invalid!");
		return false;
	}
	// Count the available ports containing the client name as device
	// channels.	Jack "input ports" equal RtAudio output channels.
	uint32_t nChannels = 0;
	uint64_t flag = JackPortIsInput;
	if (_mode == INPUT) flag = JackPortIsOutput;
	ports = jack_get_ports(client, deviceName.c_str(), nullptr, flag);
	if (ports) {
		while (ports[ nChannels ]) {
			nChannels++;
		}
		free(ports);
	}
	// Compare the jack ports for specified client to the requested number of channels.
	if (nChannels < (_channels + _firstChannel)) {
		ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: requested number of channels (" << _channels << ") + offset (" << _firstChannel << ") not found for specified device (" << _device << ":" << deviceName << ").");
		return false;
	}
	// Check the jack server sample rate.
	uint32_t jackRate = jack_get_sample_rate(client);
	if (_sampleRate != jackRate) {
		jack_client_close(client);
		ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: the requested sample rate (" << _sampleRate << ") is different than the JACK server rate (" << jackRate << ").");
		return false;
	}
	m_stream.sampleRate = jackRate;
	// Get the latency of the JACK port.
	ports = jack_get_ports(client, deviceName.c_str(), nullptr, flag);
	if (ports[ _firstChannel ]) {
		// Added by Ge Wang
		jack_latency_callback_mode_t cbmode = (_mode == INPUT ? JackCaptureLatency : JackPlaybackLatency);
		// the range (usually the min and max are equal)
		jack_latency_range_t latrange; latrange.min = latrange.max = 0;
		// get the latency range
		jack_port_get_latency_range(jack_port_by_name(client, ports[_firstChannel]), cbmode, &latrange);
		// be optimistic, use the min!
		m_stream.latency[_mode] = latrange.min;
		//m_stream.latency[_mode] = jack_port_get_latency(jack_port_by_name(client, ports[ _firstChannel ]));
	}
	free(ports);
	// The jack server always uses 32-bit floating-point data.
	m_stream.deviceFormat[_mode] = FLOAT32;
	m_stream.userFormat = _format;
	if (_options && _options->flags & NONINTERLEAVED) {
		m_stream.userInterleaved = false;
	} else {
		m_stream.userInterleaved = true;
	}
	// Jack always uses non-interleaved buffers.
	m_stream.deviceInterleaved[_mode] = false;
	// Jack always provides host byte-ordered data.
	m_stream.doByteSwap[_mode] = false;
	// Get the buffer size.	The buffer size and number of buffers
	// (periods) is set when the jack server is started.
	m_stream.bufferSize = (int) jack_get_buffer_size(client);
	*_bufferSize = m_stream.bufferSize;
	m_stream.nDeviceChannels[_mode] = _channels;
	m_stream.nUserChannels[_mode] = _channels;
	// Set flags for buffer conversion.
	m_stream.doConvertBuffer[_mode] = false;
	if (m_stream.userFormat != m_stream.deviceFormat[_mode]) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	if (    m_stream.userInterleaved != m_stream.deviceInterleaved[_mode]
	     && m_stream.nUserChannels[_mode] > 1) {
		m_stream.doConvertBuffer[_mode] = true;
	}
	// Allocate our JackHandle structure for the stream.
	if (handle == 0) {
		handle = new JackHandle;
		if (handle == nullptr) {
			ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: error allocating JackHandle memory.");
			goto error;
		}
		m_stream.apiHandle = (void *) handle;
		handle->client = client;
	}
	handle->deviceName[_mode] = deviceName;
	// Allocate necessary internal buffers.
	uint64_t bufferBytes;
	bufferBytes = m_stream.nUserChannels[_mode] * *_bufferSize * formatBytes(m_stream.userFormat);
	m_stream.userBuffer[_mode] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[_mode] == nullptr) {
		ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: error allocating user buffer memory.");
		goto error;
	}
	if (m_stream.doConvertBuffer[_mode]) {
		bool makeBuffer = true;
		if (_mode == OUTPUT) {
			bufferBytes = m_stream.nDeviceChannels[0] * formatBytes(m_stream.deviceFormat[0]);
		} else { // _mode == INPUT
			bufferBytes = m_stream.nDeviceChannels[1] * formatBytes(m_stream.deviceFormat[1]);
			if (m_stream.mode == OUTPUT && m_stream.deviceBuffer) {
				uint64_t bytesOut = m_stream.nDeviceChannels[0] * formatBytes(m_stream.deviceFormat[0]);
				if (bufferBytes < bytesOut) {
					makeBuffer = false;
				}
			}
		}
		if (makeBuffer) {
			bufferBytes *= *_bufferSize;
			if (m_stream.deviceBuffer) free(m_stream.deviceBuffer);
			m_stream.deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_stream.deviceBuffer == nullptr) {
				ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: error allocating device buffer memory.");
				goto error;
			}
		}
	}
	// Allocate memory for the Jack ports (channels) identifiers.
	handle->ports[_mode] = (jack_port_t **) malloc (sizeof (jack_port_t *) * _channels);
	if (handle->ports[_mode] == nullptr)	{
		ATA_ERROR("airtaudio::api::Jack::probeDeviceOpen: error allocating port memory.");
		goto error;
	}
	m_stream.device[_mode] = _device;
	m_stream.channelOffset[_mode] = _firstChannel;
	m_stream.state = STREAM_STOPPED;
	m_stream.callbackInfo.object = (void *) this;
	if (    m_stream.mode == OUTPUT
	     && _mode == INPUT) {
		// We had already set up the stream for output.
		m_stream.mode = DUPLEX;
	} else {
		m_stream.mode = _mode;
		jack_set_process_callback(handle->client, jackCallbackHandler, (void *) &m_stream.callbackInfo);
		jack_set_xrun_callback(handle->client, jackXrun, (void *) &handle);
		jack_on_shutdown(handle->client, jackShutdown, (void *) &m_stream.callbackInfo);
	}
	// Register our ports.
	char label[64];
	if (_mode == OUTPUT) {
		for (uint32_t i=0; i<m_stream.nUserChannels[0]; i++) {
			snprintf(label, 64, "outport %d", i);
			handle->ports[0][i] = jack_port_register(handle->client,
			                                         (const char *)label,
			                                         JACK_DEFAULT_AUDIO_TYPE,
			                                         JackPortIsOutput,
			                                         0);
		}
	} else {
		for (uint32_t i=0; i<m_stream.nUserChannels[1]; i++) {
			snprintf(label, 64, "inport %d", i);
			handle->ports[1][i] = jack_port_register(handle->client,
			                                         (const char *)label,
			                                         JACK_DEFAULT_AUDIO_TYPE,
			                                         JackPortIsInput,
			                                         0);
		}
	}
	// Setup the buffer conversion information structure.	We don't use
	// buffers to do channel offsets, so we override that parameter
	// here.
	if (m_stream.doConvertBuffer[_mode]) {
		setConvertInfo(_mode, 0);
	}
	return true;
error:
	if (handle) {
		jack_client_close(handle->client);
		if (handle->ports[0]) {
			free(handle->ports[0]);
		}
		if (handle->ports[1]) {
			free(handle->ports[1]);
		}
		delete handle;
		m_stream.apiHandle = nullptr;
	}
	for (int32_t iii=0; iii<2; ++iii) {
		if (m_stream.userBuffer[iii]) {
			free(m_stream.userBuffer[iii]);
			m_stream.userBuffer[iii] = nullptr;
		}
	}
	if (m_stream.deviceBuffer) {
		free(m_stream.deviceBuffer);
		m_stream.deviceBuffer = nullptr;
	}
	return false;
}

enum airtaudio::errorType airtaudio::api::Jack::closeStream() {
	if (m_stream.state == STREAM_CLOSED) {
		ATA_ERROR("airtaudio::api::Jack::closeStream(): no open stream to close!");
		return airtaudio::errorWarning;
	}
	JackHandle *handle = (JackHandle *) m_stream.apiHandle;
	if (handle != nullptr) {
		if (m_stream.state == STREAM_RUNNING) {
			jack_deactivate(handle->client);
		}
		jack_client_close(handle->client);
	}
	if (handle != nullptr) {
		if (handle->ports[0]) {
			free(handle->ports[0]);
		}
		if (handle->ports[1]) {
			free(handle->ports[1]);
		}
		delete handle;
		m_stream.apiHandle = nullptr;
	}
	for (int32_t i=0; i<2; i++) {
		if (m_stream.userBuffer[i]) {
			free(m_stream.userBuffer[i]);
			m_stream.userBuffer[i] = nullptr;
		}
	}
	if (m_stream.deviceBuffer) {
		free(m_stream.deviceBuffer);
		m_stream.deviceBuffer = nullptr;
	}
	m_stream.mode = UNINITIALIZED;
	m_stream.state = STREAM_CLOSED;
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::Jack::startStream() {
	if (verifyStream() != airtaudio::errorNone) {
		return airtaudio::errorFail;
	}
	if (m_stream.state == STREAM_RUNNING) {
		ATA_ERROR("airtaudio::api::Jack::startStream(): the stream is already running!");
		return airtaudio::errorWarning;
	}
	JackHandle *handle = (JackHandle *) m_stream.apiHandle;
	int32_t result = jack_activate(handle->client);
	if (result) {
		ATA_ERROR("airtaudio::api::Jack::startStream(): unable to activate JACK client!");
		goto unlock;
	}
	const char **ports;
	// Get the list of available ports.
	if (    m_stream.mode == OUTPUT
	     || m_stream.mode == DUPLEX) {
		result = 1;
		ports = jack_get_ports(handle->client, handle->deviceName[0].c_str(), nullptr, JackPortIsInput);
		if (ports == nullptr) {
			ATA_ERROR("airtaudio::api::Jack::startStream(): error determining available JACK input ports!");
			goto unlock;
		}
		// Now make the port connections.	Since RtAudio wasn't designed to
		// allow the user to select particular channels of a device, we'll
		// just open the first "nChannels" ports with offset.
		for (uint32_t i=0; i<m_stream.nUserChannels[0]; i++) {
			result = 1;
			if (ports[ m_stream.channelOffset[0] + i ])
				result = jack_connect(handle->client, jack_port_name(handle->ports[0][i]), ports[ m_stream.channelOffset[0] + i ]);
			if (result) {
				free(ports);
				ATA_ERROR("airtaudio::api::Jack::startStream(): error connecting output ports!");
				goto unlock;
			}
		}
		free(ports);
	}
	if (    m_stream.mode == INPUT
	     || m_stream.mode == DUPLEX) {
		result = 1;
		ports = jack_get_ports(handle->client, handle->deviceName[1].c_str(), nullptr, JackPortIsOutput);
		if (ports == nullptr) {
			ATA_ERROR("airtaudio::api::Jack::startStream(): error determining available JACK output ports!");
			goto unlock;
		}
		// Now make the port connections. See note above.
		for (uint32_t i=0; i<m_stream.nUserChannels[1]; i++) {
			result = 1;
			if (ports[ m_stream.channelOffset[1] + i ]) {
				result = jack_connect(handle->client, ports[ m_stream.channelOffset[1] + i ], jack_port_name(handle->ports[1][i]));
			}
			if (result) {
				free(ports);
				ATA_ERROR("airtaudio::api::Jack::startStream(): error connecting input ports!");
				goto unlock;
			}
		}
		free(ports);
	}
	handle->drainCounter = 0;
	handle->internalDrain = false;
	m_stream.state = STREAM_RUNNING;
unlock:
	if (result == 0) {
		return airtaudio::errorNone;
	}
	return airtaudio::errorSystemError;
}

enum airtaudio::errorType airtaudio::api::Jack::stopStream() {
	if (verifyStream() != airtaudio::errorNone) {
		return airtaudio::errorFail;
	}
	if (m_stream.state == STREAM_STOPPED) {
		ATA_ERROR("airtaudio::api::Jack::stopStream(): the stream is already stopped!");
		return airtaudio::errorWarning;
	}
	JackHandle *handle = (JackHandle *) m_stream.apiHandle;
	if (    m_stream.mode == OUTPUT
	     || m_stream.mode == DUPLEX) {
		if (handle->drainCounter == 0) {
			handle->drainCounter = 2;
			std::unique_lock<std::mutex> lck(m_stream.mutex);
			handle->condition.wait(lck);
		}
	}
	jack_deactivate(handle->client);
	m_stream.state = STREAM_STOPPED;
	return airtaudio::errorNone;
}

enum airtaudio::errorType airtaudio::api::Jack::abortStream() {
	if (verifyStream() != airtaudio::errorNone) {
		return airtaudio::errorFail;
	}
	if (m_stream.state == STREAM_STOPPED) {
		ATA_ERROR("airtaudio::api::Jack::abortStream(): the stream is already stopped!");
		return airtaudio::errorWarning;
	}
	JackHandle *handle = (JackHandle *) m_stream.apiHandle;
	handle->drainCounter = 2;
	return stopStream();
}

// This function will be called by a spawned thread when the user
// callback function signals that the stream should be stopped or
// aborted.	It is necessary to handle it this way because the
// callbackEvent() function must return before the jack_deactivate()
// function will return.
static void jackStopStream(void *_ptr) {
	airtaudio::CallbackInfo *info = (airtaudio::CallbackInfo *) _ptr;
	airtaudio::api::Jack *object = (airtaudio::api::Jack *) info->object;
	object->stopStream();
}

bool airtaudio::api::Jack::callbackEvent(uint64_t _nframes) {
	if (    m_stream.state == STREAM_STOPPED
	     || m_stream.state == STREAM_STOPPING) {
		return true;
	}
	if (m_stream.state == STREAM_CLOSED) {
		ATA_ERROR("RtApiCore::callbackEvent(): the stream is closed ... this shouldn't happen!");
		return false;
	}
	if (m_stream.bufferSize != _nframes) {
		ATA_ERROR("RtApiCore::callbackEvent(): the JACK buffer size has changed ... cannot process!");
		return false;
	}
	CallbackInfo *info = (CallbackInfo *) &m_stream.callbackInfo;
	JackHandle *handle = (JackHandle *) m_stream.apiHandle;
	// Check if we were draining the stream and signal is finished.
	if (handle->drainCounter > 3) {
		m_stream.state = STREAM_STOPPING;
		if (handle->internalDrain == true) {
			new std::thread(jackStopStream, info);
		} else {
			handle->condition.notify_one();
		}
		return true;
	}
	// Invoke user callback first, to get fresh output data.
	if (handle->drainCounter == 0) {
		double streamTime = getStreamTime();
		airtaudio::streamStatus status = 0;
		if (m_stream.mode != INPUT && handle->xrun[0] == true) {
			status |= OUTPUT_UNDERFLOW;
			handle->xrun[0] = false;
		}
		if (m_stream.mode != OUTPUT && handle->xrun[1] == true) {
			status |= INPUT_OVERFLOW;
			handle->xrun[1] = false;
		}
		int32_t cbReturnValue = info->callback(m_stream.userBuffer[0],
		                                       m_stream.userBuffer[1],
		                                       m_stream.bufferSize,
		                                       streamTime,
		                                       status);
		if (cbReturnValue == 2) {
			m_stream.state = STREAM_STOPPING;
			handle->drainCounter = 2;
			new std::thread(jackStopStream, info);
			return true;
		}
		else if (cbReturnValue == 1) {
			handle->drainCounter = 1;
			handle->internalDrain = true;
		}
	}
	jack_default_audio_sample_t *jackbuffer;
	uint64_t bufferBytes = _nframes * sizeof(jack_default_audio_sample_t);
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {
		if (handle->drainCounter > 1) { // write zeros to the output stream
			for (uint32_t i=0; i<m_stream.nDeviceChannels[0]; i++) {
				jackbuffer = (jack_default_audio_sample_t *) jack_port_get_buffer(handle->ports[0][i], (jack_nframes_t) _nframes);
				memset(jackbuffer, 0, bufferBytes);
			}
		} else if (m_stream.doConvertBuffer[0]) {
			convertBuffer(m_stream.deviceBuffer, m_stream.userBuffer[0], m_stream.convertInfo[0]);
			for (uint32_t i=0; i<m_stream.nDeviceChannels[0]; i++) {
				jackbuffer = (jack_default_audio_sample_t *) jack_port_get_buffer(handle->ports[0][i], (jack_nframes_t) _nframes);
				memcpy(jackbuffer, &m_stream.deviceBuffer[i*bufferBytes], bufferBytes);
			}
		} else { // no buffer conversion
			for (uint32_t i=0; i<m_stream.nUserChannels[0]; i++) {
				jackbuffer = (jack_default_audio_sample_t *) jack_port_get_buffer(handle->ports[0][i], (jack_nframes_t) _nframes);
				memcpy(jackbuffer, &m_stream.userBuffer[0][i*bufferBytes], bufferBytes);
			}
		}
		if (handle->drainCounter) {
			handle->drainCounter++;
			goto unlock;
		}
	}
	if (    m_stream.mode == INPUT
	     || m_stream.mode == DUPLEX) {
		if (m_stream.doConvertBuffer[1]) {
			for (uint32_t i=0; i<m_stream.nDeviceChannels[1]; i++) {
				jackbuffer = (jack_default_audio_sample_t *) jack_port_get_buffer(handle->ports[1][i], (jack_nframes_t) _nframes);
				memcpy(&m_stream.deviceBuffer[i*bufferBytes], jackbuffer, bufferBytes);
			}
			convertBuffer(m_stream.userBuffer[1], m_stream.deviceBuffer, m_stream.convertInfo[1]);
		} else {
			// no buffer conversion
			for (uint32_t i=0; i<m_stream.nUserChannels[1]; i++) {
				jackbuffer = (jack_default_audio_sample_t *) jack_port_get_buffer(handle->ports[1][i], (jack_nframes_t) _nframes);
				memcpy(&m_stream.userBuffer[1][i*bufferBytes], jackbuffer, bufferBytes);
			}
		}
	}
unlock:
	airtaudio::Api::tickStreamTime();
	return true;
}

#endif

