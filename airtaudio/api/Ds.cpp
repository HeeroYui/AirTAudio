/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

// Windows DirectSound API
#if defined(__WINDOWS_DS__)
#include <airtaudio/Interface.h>

airtaudio::Api* airtaudio::api::Ds::Create(void) {
	return new airtaudio::api::Ds();
}


// Modified by Robin Davies, October 2005
// - Improvements to DirectX pointer chasing. 
// - Bug fix for non-power-of-two Asio granularity used by Edirol PCR-A30.
// - Auto-call CoInitialize for DSOUND and ASIO platforms.
// Various revisions for RtAudio 4.0 by Gary Scavone, April 2007
// Changed device query structure for RtAudio 4.0.7, January 2010

#include <dsound.h>
#include <assert.h>
#include <algorithm>

#if defined(__MINGW32__)
	// missing from latest mingw winapi
#define WAVE_FORMAT_96M08 0x00010000 /* 96 kHz, Mono, 8-bit */
#define WAVE_FORMAT_96S08 0x00020000 /* 96 kHz, Stereo, 8-bit */
#define WAVE_FORMAT_96M16 0x00040000 /* 96 kHz, Mono, 16-bit */
#define WAVE_FORMAT_96S16 0x00080000 /* 96 kHz, Stereo, 16-bit */
#endif

#define MINIMUM_DEVICE_BUFFER_SIZE 32768

#ifdef _MSC_VER // if Microsoft Visual C++
#pragma comment(lib, "winmm.lib") // then, auto-link winmm.lib. Otherwise, it has to be added manually.
#endif

static inline DWORD dsPointerBetween(DWORD pointer, DWORD laterPointer, DWORD earlierPointer, DWORD bufferSize)
{
	if (pointer > bufferSize) pointer -= bufferSize;
	if (laterPointer < earlierPointer) laterPointer += bufferSize;
	if (pointer < earlierPointer) pointer += bufferSize;
	return pointer >= earlierPointer && pointer < laterPointer;
}

// A structure to hold various information related to the DirectSound
// API implementation.
struct DsHandle {
	uint32_t drainCounter; // Tracks callback counts when draining
	bool internalDrain; // Indicates if stop is initiated from callback or not.
	void *id[2];
	void *buffer[2];
	bool xrun[2];
	UINT bufferPointer[2];	
	DWORD dsBufferSize[2];
	DWORD dsPointerLeadTime[2]; // the number of bytes ahead of the safe pointer to lead by.
	HANDLE condition;

	DsHandle(void) :
	  drainCounter(0),
	  internalDrain(false) {
		id[0] = 0;
		id[1] = 0;
		buffer[0] = 0;
		buffer[1] = 0;
		xrun[0] = false;
		xrun[1] = false;
		bufferPointer[0] = 0;
		bufferPointer[1] = 0;
	}
};

// Declarations for utility functions, callbacks, and structures
// specific to the DirectSound implementation.
static BOOL CALLBACK deviceQueryCallback(LPGUID _lpguid,
                                         LPCTSTR _description,
                                         LPCTSTR _module,
                                         LPVOID _lpContext);

static const char* getErrorString(int32_t code);

static unsigned __stdcall callbackHandler(void *ptr);

struct DsDevice {
	LPGUID id[2];
	bool validId[2];
	bool found;
	std::string name;

	DsDevice()
	: found(false) { validId[0] = false; validId[1] = false; }
};

struct DsProbeData {
	bool isInput;
	std::vector<struct DsDevice>* dsDevices;
};

airtaudio::api::Ds::Ds(void) {
	// Dsound will run both-threaded. If CoInitialize fails, then just
	// accept whatever the mainline chose for a threading model.
	m_coInitialized = false;
	HRESULT hr = CoInitialize(NULL);
	if (!FAILED(hr)) m_coInitialized = true;
}

airtaudio::api::Ds::~Ds(void) {
	if (m_coInitialized) {
		CoUninitialize(); // balanced call.
	}
	if (m_stream.state != STREAM_CLOSED) {
		closeStream();
	}
}

// The DirectSound default output is always the first device.
uint32_t airtaudio::api::Ds::getDefaultOutputDevice(void) {
	return 0;
}

// The DirectSound default input is always the first input device,
// which is the first capture device enumerated.
uint32_t airtaudio::api::Ds::getDefaultInputDevice(void) {
	return 0;
}

uint32_t airtaudio::api::Ds::getDeviceCount(void) {
	// Set query flag for previously found devices to false, so that we
	// can check for any devices that have disappeared.
	for (uint32_t i=0; i<dsDevices.size(); i++)
		dsDevices[i].found = false;

	// Query DirectSound devices.
	struct DsProbeData probeInfo;
	probeInfo.isInput = false;
	probeInfo.dsDevices = &dsDevices;
	HRESULT result = DirectSoundEnumerate((LPDSENUMCALLBACK) deviceQueryCallback, &probeInfo);
	if (FAILED(result)) {
		m_errorStream << "airtaudio::api::Ds::getDeviceCount: error (" << getErrorString(result) << ") enumerating output devices!";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
	}

	// Query DirectSoundCapture devices.
	probeInfo.isInput = true;
	result = DirectSoundCaptureEnumerate((LPDSENUMCALLBACK) deviceQueryCallback, &probeInfo);
	if (FAILED(result)) {
		m_errorStream << "airtaudio::api::Ds::getDeviceCount: error (" << getErrorString(result) << ") enumerating input devices!";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
	}

	// Clean out any devices that may have disappeared.
	std::vector< int32_t > indices;
	for (uint32_t i=0; i<dsDevices.size(); i++)
		if (dsDevices[i].found == false) indices.push_back(i);
	uint32_t nErased = 0;
	for (uint32_t i=0; i<indices.size(); i++)
		dsDevices.erase(dsDevices.begin()-nErased++);

	return dsDevices.size();
}

rtaudio::DeviceInfo airtaudio::api::Ds::getDeviceInfo(uint32_t device)
{
	rtaudio::DeviceInfo info;
	info.probed = false;

	if (dsDevices.size() == 0) {
		// Force a query of all devices
		getDeviceCount();
		if (dsDevices.size() == 0) {
			m_errorText = "airtaudio::api::Ds::getDeviceInfo: no devices found!";
			error(airtaudio::errorInvalidUse);
			return info;
		}
	}

	if (device >= dsDevices.size()) {
		m_errorText = "airtaudio::api::Ds::getDeviceInfo: device ID is invalid!";
		error(airtaudio::errorInvalidUse);
		return info;
	}

	HRESULT result;
	if (dsDevices[ device ].validId[0] == false) goto probeInput;

	LPDIRECTSOUND output;
	DSCAPS outCaps;
	result = DirectSoundCreate(dsDevices[ device ].id[0], &output, NULL);
	if (FAILED(result)) {
		m_errorStream << "airtaudio::api::Ds::getDeviceInfo: error (" << getErrorString(result) << ") opening output device (" << dsDevices[ device ].name << ")!";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		goto probeInput;
	}

	outCaps.dwSize = sizeof(outCaps);
	result = output->GetCaps(&outCaps);
	if (FAILED(result)) {
		output->Release();
		m_errorStream << "airtaudio::api::Ds::getDeviceInfo: error (" << getErrorString(result) << ") getting capabilities!";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		goto probeInput;
	}

	// Get output channel information.
	info.outputChannels = (outCaps.dwFlags & DSCAPS_PRIMARYSTEREO) ? 2 : 1;

	// Get sample rate information.
	info.sampleRates.clear();
	for (uint32_t k=0; k<MAX_SAMPLE_RATES; k++) {
		if (SAMPLE_RATES[k] >= (uint32_t) outCaps.dwMinSecondarySampleRate &&
				 SAMPLE_RATES[k] <= (uint32_t) outCaps.dwMaxSecondarySampleRate)
			info.sampleRates.push_back(SAMPLE_RATES[k]);
	}

	// Get format information.
	if (outCaps.dwFlags & DSCAPS_PRIMARY16BIT) info.nativeFormats |= RTAUDIO_SINT16;
	if (outCaps.dwFlags & DSCAPS_PRIMARY8BIT) info.nativeFormats |= RTAUDIO_SINT8;

	output->Release();

	if (getDefaultOutputDevice() == device)
		info.isDefaultOutput = true;

	if (dsDevices[ device ].validId[1] == false) {
		info.name = dsDevices[ device ].name;
		info.probed = true;
		return info;
	}

 probeInput:

	LPDIRECTSOUNDCAPTURE input;
	result = DirectSoundCaptureCreate(dsDevices[ device ].id[1], &input, NULL);
	if (FAILED(result)) {
		m_errorStream << "airtaudio::api::Ds::getDeviceInfo: error (" << getErrorString(result) << ") opening input device (" << dsDevices[ device ].name << ")!";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	DSCCAPS inCaps;
	inCaps.dwSize = sizeof(inCaps);
	result = input->GetCaps(&inCaps);
	if (FAILED(result)) {
		input->Release();
		m_errorStream << "airtaudio::api::Ds::getDeviceInfo: error (" << getErrorString(result) << ") getting object capabilities (" << dsDevices[ device ].name << ")!";
		m_errorText = m_errorStream.str();
		error(airtaudio::errorWarning);
		return info;
	}

	// Get input channel information.
	info.inputChannels = inCaps.dwChannels;

	// Get sample rate and format information.
	std::vector<uint32_t> rates;
	if (inCaps.dwChannels >= 2) {
		if (inCaps.dwFormats & WAVE_FORMAT_1S16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_2S16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_4S16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_96S16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_1S08) info.nativeFormats |= RTAUDIO_SINT8;
		if (inCaps.dwFormats & WAVE_FORMAT_2S08) info.nativeFormats |= RTAUDIO_SINT8;
		if (inCaps.dwFormats & WAVE_FORMAT_4S08) info.nativeFormats |= RTAUDIO_SINT8;
		if (inCaps.dwFormats & WAVE_FORMAT_96S08) info.nativeFormats |= RTAUDIO_SINT8;

		if (info.nativeFormats & RTAUDIO_SINT16) {
			if (inCaps.dwFormats & WAVE_FORMAT_1S16) rates.push_back(11025);
			if (inCaps.dwFormats & WAVE_FORMAT_2S16) rates.push_back(22050);
			if (inCaps.dwFormats & WAVE_FORMAT_4S16) rates.push_back(44100);
			if (inCaps.dwFormats & WAVE_FORMAT_96S16) rates.push_back(96000);
		}
		else if (info.nativeFormats & RTAUDIO_SINT8) {
			if (inCaps.dwFormats & WAVE_FORMAT_1S08) rates.push_back(11025);
			if (inCaps.dwFormats & WAVE_FORMAT_2S08) rates.push_back(22050);
			if (inCaps.dwFormats & WAVE_FORMAT_4S08) rates.push_back(44100);
			if (inCaps.dwFormats & WAVE_FORMAT_96S08) rates.push_back(96000);
		}
	}
	else if (inCaps.dwChannels == 1) {
		if (inCaps.dwFormats & WAVE_FORMAT_1M16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_2M16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_4M16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_96M16) info.nativeFormats |= RTAUDIO_SINT16;
		if (inCaps.dwFormats & WAVE_FORMAT_1M08) info.nativeFormats |= RTAUDIO_SINT8;
		if (inCaps.dwFormats & WAVE_FORMAT_2M08) info.nativeFormats |= RTAUDIO_SINT8;
		if (inCaps.dwFormats & WAVE_FORMAT_4M08) info.nativeFormats |= RTAUDIO_SINT8;
		if (inCaps.dwFormats & WAVE_FORMAT_96M08) info.nativeFormats |= RTAUDIO_SINT8;

		if (info.nativeFormats & RTAUDIO_SINT16) {
			if (inCaps.dwFormats & WAVE_FORMAT_1M16) rates.push_back(11025);
			if (inCaps.dwFormats & WAVE_FORMAT_2M16) rates.push_back(22050);
			if (inCaps.dwFormats & WAVE_FORMAT_4M16) rates.push_back(44100);
			if (inCaps.dwFormats & WAVE_FORMAT_96M16) rates.push_back(96000);
		}
		else if (info.nativeFormats & RTAUDIO_SINT8) {
			if (inCaps.dwFormats & WAVE_FORMAT_1M08) rates.push_back(11025);
			if (inCaps.dwFormats & WAVE_FORMAT_2M08) rates.push_back(22050);
			if (inCaps.dwFormats & WAVE_FORMAT_4M08) rates.push_back(44100);
			if (inCaps.dwFormats & WAVE_FORMAT_96M08) rates.push_back(96000);
		}
	}
	else info.inputChannels = 0; // technically, this would be an error

	input->Release();

	if (info.inputChannels == 0) return info;

	// Copy the supported rates to the info structure but avoid duplication.
	bool found;
	for (uint32_t i=0; i<rates.size(); i++) {
		found = false;
		for (uint32_t j=0; j<info.sampleRates.size(); j++) {
			if (rates[i] == info.sampleRates[j]) {
				found = true;
				break;
			}
		}
		if (found == false) info.sampleRates.push_back(rates[i]);
	}
	std::sort(info.sampleRates.begin(), info.sampleRates.end());

	// If device opens for both playback and capture, we determine the channels.
	if (info.outputChannels > 0 && info.inputChannels > 0)
		info.duplexChannels = (info.outputChannels > info.inputChannels) ? info.inputChannels : info.outputChannels;

	if (device == 0) info.isDefaultInput = true;

	// Copy name and return.
	info.name = dsDevices[ device ].name;
	info.probed = true;
	return info;
}

bool airtaudio::api::Ds::probeDeviceOpen(uint32_t device, StreamMode mode, uint32_t channels,
																 uint32_t firstChannel, uint32_t sampleRate,
																 rtaudio::format format, uint32_t *bufferSize,
																 rtaudio::StreamOptions *options)
{
	if (channels + firstChannel > 2) {
		m_errorText = "airtaudio::api::Ds::probeDeviceOpen: DirectSound does not support more than 2 channels per device.";
		return FAILURE;
	}

	uint32_t nDevices = dsDevices.size();
	if (nDevices == 0) {
		// This should not happen because a check is made before this function is called.
		m_errorText = "airtaudio::api::Ds::probeDeviceOpen: no devices found!";
		return FAILURE;
	}

	if (device >= nDevices) {
		// This should not happen because a check is made before this function is called.
		m_errorText = "airtaudio::api::Ds::probeDeviceOpen: device ID is invalid!";
		return FAILURE;
	}

	if (mode == OUTPUT) {
		if (dsDevices[ device ].validId[0] == false) {
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: device (" << device << ") does not support output!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}
	}
	else { // mode == INPUT
		if (dsDevices[ device ].validId[1] == false) {
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: device (" << device << ") does not support input!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}
	}

	// According to a note in PortAudio, using GetDesktopWindow()
	// instead of GetForegroundWindow() is supposed to avoid problems
	// that occur when the application's window is not the foreground
	// window.	Also, if the application window closes before the
	// DirectSound buffer, DirectSound can crash.	In the past, I had
	// problems when using GetDesktopWindow() but it seems fine now
	// (January 2010).	I'll leave it commented here.
	// HWND hWnd = GetForegroundWindow();
	HWND hWnd = GetDesktopWindow();

	// Check the numberOfBuffers parameter and limit the lowest value to
	// two.	This is a judgement call and a value of two is probably too
	// low for capture, but it should work for playback.
	int32_t nBuffers = 0;
	if (options) nBuffers = options->numberOfBuffers;
	if (options && options->flags & RTAUDIO_MINIMIZE_LATENCY) nBuffers = 2;
	if (nBuffers < 2) nBuffers = 3;

	// Check the lower range of the user-specified buffer size and set
	// (arbitrarily) to a lower bound of 32.
	if (*bufferSize < 32) *bufferSize = 32;

	// Create the wave format structure.	The data format setting will
	// be determined later.
	WAVEFORMATEX waveFormat;
	ZeroMemory(&waveFormat, sizeof(WAVEFORMATEX));
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = channels + firstChannel;
	waveFormat.nSamplesPerSec = (uint64_t) sampleRate;

	// Determine the device buffer size. By default, we'll use the value
	// defined above (32K), but we will grow it to make allowances for
	// very large software buffer sizes.
	DWORD dsBufferSize = MINIMUM_DEVICE_BUFFER_SIZE;
	DWORD dsPointerLeadTime = 0;

	void *ohandle = 0, *bhandle = 0;
	HRESULT result;
	if (mode == OUTPUT) {

		LPDIRECTSOUND output;
		result = DirectSoundCreate(dsDevices[ device ].id[0], &output, NULL);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") opening output device (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		DSCAPS outCaps;
		outCaps.dwSize = sizeof(outCaps);
		result = output->GetCaps(&outCaps);
		if (FAILED(result)) {
			output->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") getting capabilities (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Check channel information.
		if (channels + firstChannel == 2 && !(outCaps.dwFlags & DSCAPS_PRIMARYSTEREO)) {
			m_errorStream << "airtaudio::api::Ds::getDeviceInfo: the output device (" << dsDevices[ device ].name << ") does not support stereo playback.";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Check format information.	Use 16-bit format unless not
		// supported or user requests 8-bit.
		if (outCaps.dwFlags & DSCAPS_PRIMARY16BIT &&
				 !(format == RTAUDIO_SINT8 && outCaps.dwFlags & DSCAPS_PRIMARY8BIT)) {
			waveFormat.wBitsPerSample = 16;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
		}
		else {
			waveFormat.wBitsPerSample = 8;
			m_stream.deviceFormat[mode] = RTAUDIO_SINT8;
		}
		m_stream.userFormat = format;

		// Update wave format structure and buffer information.
		waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		dsPointerLeadTime = nBuffers * (*bufferSize) * (waveFormat.wBitsPerSample / 8) * channels;

		// If the user wants an even bigger buffer, increase the device buffer size accordingly.
		while (dsPointerLeadTime * 2U > dsBufferSize)
			dsBufferSize *= 2;

		// Set cooperative level to DSSCL_EXCLUSIVE ... sound stops when window focus changes.
		// result = output->SetCooperativeLevel(hWnd, DSSCL_EXCLUSIVE);
		// Set cooperative level to DSSCL_PRIORITY ... sound remains when window focus changes.
		result = output->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
		if (FAILED(result)) {
			output->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") setting cooperative level (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Even though we will write to the secondary buffer, we need to
		// access the primary buffer to set the correct output format
		// (since the default is 8-bit, 22 kHz!).	Setup the DS primary
		// buffer description.
		DSBUFFERDESC bufferDescription;
		ZeroMemory(&bufferDescription, sizeof(DSBUFFERDESC));
		bufferDescription.dwSize = sizeof(DSBUFFERDESC);
		bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

		// Obtain the primary buffer
		LPDIRECTSOUNDBUFFER buffer;
		result = output->CreateSoundBuffer(&bufferDescription, &buffer, NULL);
		if (FAILED(result)) {
			output->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") accessing primary buffer (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Set the primary DS buffer sound format.
		result = buffer->SetFormat(&waveFormat);
		if (FAILED(result)) {
			output->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") setting primary buffer format (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Setup the secondary DS buffer description.
		ZeroMemory(&bufferDescription, sizeof(DSBUFFERDESC));
		bufferDescription.dwSize = sizeof(DSBUFFERDESC);
		bufferDescription.dwFlags = (DSBCAPS_STICKYFOCUS |
																	DSBCAPS_GLOBALFOCUS |
																	DSBCAPS_GETCURRENTPOSITION2 |
																	DSBCAPS_LOCHARDWARE);	// Force hardware mixing
		bufferDescription.dwBufferBytes = dsBufferSize;
		bufferDescription.lpwfxFormat = &waveFormat;

		// Try to create the secondary DS buffer.	If that doesn't work,
		// try to use software mixing.	Otherwise, there's a problem.
		result = output->CreateSoundBuffer(&bufferDescription, &buffer, NULL);
		if (FAILED(result)) {
			bufferDescription.dwFlags = (DSBCAPS_STICKYFOCUS |
																		DSBCAPS_GLOBALFOCUS |
																		DSBCAPS_GETCURRENTPOSITION2 |
																		DSBCAPS_LOCSOFTWARE);	// Force software mixing
			result = output->CreateSoundBuffer(&bufferDescription, &buffer, NULL);
			if (FAILED(result)) {
				output->Release();
				m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") creating secondary buffer (" << dsDevices[ device ].name << ")!";
				m_errorText = m_errorStream.str();
				return FAILURE;
			}
		}

		// Get the buffer size ... might be different from what we specified.
		DSBCAPS dsbcaps;
		dsbcaps.dwSize = sizeof(DSBCAPS);
		result = buffer->GetCaps(&dsbcaps);
		if (FAILED(result)) {
			output->Release();
			buffer->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") getting buffer settings (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		dsBufferSize = dsbcaps.dwBufferBytes;

		// Lock the DS buffer
		LPVOID audioPtr;
		DWORD dataLen;
		result = buffer->Lock(0, dsBufferSize, &audioPtr, &dataLen, NULL, NULL, 0);
		if (FAILED(result)) {
			output->Release();
			buffer->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") locking buffer (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Zero the DS buffer
		ZeroMemory(audioPtr, dataLen);

		// Unlock the DS buffer
		result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
		if (FAILED(result)) {
			output->Release();
			buffer->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") unlocking buffer (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		ohandle = (void *) output;
		bhandle = (void *) buffer;
	}

	if (mode == INPUT) {

		LPDIRECTSOUNDCAPTURE input;
		result = DirectSoundCaptureCreate(dsDevices[ device ].id[1], &input, NULL);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") opening input device (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		DSCCAPS inCaps;
		inCaps.dwSize = sizeof(inCaps);
		result = input->GetCaps(&inCaps);
		if (FAILED(result)) {
			input->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") getting input capabilities (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Check channel information.
		if (inCaps.dwChannels < channels + firstChannel) {
			m_errorText = "airtaudio::api::Ds::getDeviceInfo: the input device does not support requested input channels.";
			return FAILURE;
		}

		// Check format information.	Use 16-bit format unless user
		// requests 8-bit.
		DWORD deviceFormats;
		if (channels + firstChannel == 2) {
			deviceFormats = WAVE_FORMAT_1S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4S08 | WAVE_FORMAT_96S08;
			if (format == RTAUDIO_SINT8 && inCaps.dwFormats & deviceFormats) {
				waveFormat.wBitsPerSample = 8;
				m_stream.deviceFormat[mode] = RTAUDIO_SINT8;
			}
			else { // assume 16-bit is supported
				waveFormat.wBitsPerSample = 16;
				m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
			}
		}
		else { // channel == 1
			deviceFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_4M08 | WAVE_FORMAT_96M08;
			if (format == RTAUDIO_SINT8 && inCaps.dwFormats & deviceFormats) {
				waveFormat.wBitsPerSample = 8;
				m_stream.deviceFormat[mode] = RTAUDIO_SINT8;
			}
			else { // assume 16-bit is supported
				waveFormat.wBitsPerSample = 16;
				m_stream.deviceFormat[mode] = RTAUDIO_SINT16;
			}
		}
		m_stream.userFormat = format;

		// Update wave format structure and buffer information.
		waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		dsPointerLeadTime = nBuffers * (*bufferSize) * (waveFormat.wBitsPerSample / 8) * channels;

		// If the user wants an even bigger buffer, increase the device buffer size accordingly.
		while (dsPointerLeadTime * 2U > dsBufferSize)
			dsBufferSize *= 2;

		// Setup the secondary DS buffer description.
		DSCBUFFERDESC bufferDescription;
		ZeroMemory(&bufferDescription, sizeof(DSCBUFFERDESC));
		bufferDescription.dwSize = sizeof(DSCBUFFERDESC);
		bufferDescription.dwFlags = 0;
		bufferDescription.dwReserved = 0;
		bufferDescription.dwBufferBytes = dsBufferSize;
		bufferDescription.lpwfxFormat = &waveFormat;

		// Create the capture buffer.
		LPDIRECTSOUNDCAPTUREBUFFER buffer;
		result = input->CreateCaptureBuffer(&bufferDescription, &buffer, NULL);
		if (FAILED(result)) {
			input->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") creating input buffer (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Get the buffer size ... might be different from what we specified.
		DSCBCAPS dscbcaps;
		dscbcaps.dwSize = sizeof(DSCBCAPS);
		result = buffer->GetCaps(&dscbcaps);
		if (FAILED(result)) {
			input->Release();
			buffer->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") getting buffer settings (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		dsBufferSize = dscbcaps.dwBufferBytes;

		// NOTE: We could have a problem here if this is a duplex stream
		// and the play and capture hardware buffer sizes are different
		// (I'm actually not sure if that is a problem or not).
		// Currently, we are not verifying that.

		// Lock the capture buffer
		LPVOID audioPtr;
		DWORD dataLen;
		result = buffer->Lock(0, dsBufferSize, &audioPtr, &dataLen, NULL, NULL, 0);
		if (FAILED(result)) {
			input->Release();
			buffer->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") locking input buffer (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		// Zero the buffer
		ZeroMemory(audioPtr, dataLen);

		// Unlock the buffer
		result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
		if (FAILED(result)) {
			input->Release();
			buffer->Release();
			m_errorStream << "airtaudio::api::Ds::probeDeviceOpen: error (" << getErrorString(result) << ") unlocking input buffer (" << dsDevices[ device ].name << ")!";
			m_errorText = m_errorStream.str();
			return FAILURE;
		}

		ohandle = (void *) input;
		bhandle = (void *) buffer;
	}

	// Set various stream parameters
	DsHandle *handle = 0;
	m_stream.nDeviceChannels[mode] = channels + firstChannel;
	m_stream.nUserChannels[mode] = channels;
	m_stream.bufferSize = *bufferSize;
	m_stream.channelOffset[mode] = firstChannel;
	m_stream.deviceInterleaved[mode] = true;
	if (options && options->flags & RTAUDIO_NONINTERLEAVED) m_stream.userInterleaved = false;
	else m_stream.userInterleaved = true;

	// Set flag for buffer conversion
	m_stream.doConvertBuffer[mode] = false;
	if (m_stream.nUserChannels[mode] != m_stream.nDeviceChannels[mode])
		m_stream.doConvertBuffer[mode] = true;
	if (m_stream.userFormat != m_stream.deviceFormat[mode])
		m_stream.doConvertBuffer[mode] = true;
	if (m_stream.userInterleaved != m_stream.deviceInterleaved[mode] &&
			 m_stream.nUserChannels[mode] > 1)
		m_stream.doConvertBuffer[mode] = true;

	// Allocate necessary internal buffers
	long bufferBytes = m_stream.nUserChannels[mode] * *bufferSize * formatBytes(m_stream.userFormat);
	m_stream.userBuffer[mode] = (char *) calloc(bufferBytes, 1);
	if (m_stream.userBuffer[mode] == NULL) {
		m_errorText = "airtaudio::api::Ds::probeDeviceOpen: error allocating user buffer memory.";
		goto error;
	}

	if (m_stream.doConvertBuffer[mode]) {

		bool makeBuffer = true;
		bufferBytes = m_stream.nDeviceChannels[mode] * formatBytes(m_stream.deviceFormat[mode]);
		if (mode == INPUT) {
			if (m_stream.mode == OUTPUT && m_stream.deviceBuffer) {
				uint64_t bytesOut = m_stream.nDeviceChannels[0] * formatBytes(m_stream.deviceFormat[0]);
				if (bufferBytes <= (long) bytesOut) makeBuffer = false;
			}
		}

		if (makeBuffer) {
			bufferBytes *= *bufferSize;
			if (m_stream.deviceBuffer) free(m_stream.deviceBuffer);
			m_stream.deviceBuffer = (char *) calloc(bufferBytes, 1);
			if (m_stream.deviceBuffer == NULL) {
				m_errorText = "airtaudio::api::Ds::probeDeviceOpen: error allocating device buffer memory.";
				goto error;
			}
		}
	}

	// Allocate our DsHandle structures for the stream.
	if (m_stream.apiHandle == 0) {
		try {
			handle = new DsHandle;
		}
		catch (std::bad_alloc&) {
			m_errorText = "airtaudio::api::Ds::probeDeviceOpen: error allocating AsioHandle memory.";
			goto error;
		}

		// Create a manual-reset event.
		handle->condition = CreateEvent(NULL,	 // no security
																		 TRUE,	 // manual-reset
																		 FALSE,	// non-signaled initially
																		 NULL); // unnamed
		m_stream.apiHandle = (void *) handle;
	}
	else
		handle = (DsHandle *) m_stream.apiHandle;
	handle->id[mode] = ohandle;
	handle->buffer[mode] = bhandle;
	handle->dsBufferSize[mode] = dsBufferSize;
	handle->dsPointerLeadTime[mode] = dsPointerLeadTime;

	m_stream.device[mode] = device;
	m_stream.state = STREAM_STOPPED;
	if (m_stream.mode == OUTPUT && mode == INPUT)
		// We had already set up an output stream.
		m_stream.mode = DUPLEX;
	else
		m_stream.mode = mode;
	m_stream.nBuffers = nBuffers;
	m_stream.sampleRate = sampleRate;

	// Setup the buffer conversion information structure.
	if (m_stream.doConvertBuffer[mode]) setConvertInfo(mode, firstChannel);

	// Setup the callback thread.
	if (m_stream.callbackInfo.isRunning == false) {
		unsigned threadId;
		m_stream.callbackInfo.isRunning = true;
		m_stream.callbackInfo.object = (void *) this;
		m_stream.callbackInfo.thread = _beginthreadex(NULL, 0, &callbackHandler,
																									&m_stream.callbackInfo, 0, &threadId);
		if (m_stream.callbackInfo.thread == 0) {
			m_errorText = "airtaudio::api::Ds::probeDeviceOpen: error creating callback thread!";
			goto error;
		}

		// Boost DS thread priority
		SetThreadPriority((HANDLE) m_stream.callbackInfo.thread, THREAD_PRIORITY_HIGHEST);
	}
	return SUCCESS;

 error:
	if (handle) {
		if (handle->buffer[0]) { // the object pointer can be NULL and valid
			LPDIRECTSOUND object = (LPDIRECTSOUND) handle->id[0];
			LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) handle->buffer[0];
			if (buffer) buffer->Release();
			object->Release();
		}
		if (handle->buffer[1]) {
			LPDIRECTSOUNDCAPTURE object = (LPDIRECTSOUNDCAPTURE) handle->id[1];
			LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) handle->buffer[1];
			if (buffer) buffer->Release();
			object->Release();
		}
		CloseHandle(handle->condition);
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

	m_stream.state = STREAM_CLOSED;
	return FAILURE;
}

void airtaudio::api::Ds::closeStream()
{
	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Ds::closeStream(): no open stream to close!";
		error(airtaudio::errorWarning);
		return;
	}

	// Stop the callback thread.
	m_stream.callbackInfo.isRunning = false;
	WaitForSingleObject((HANDLE) m_stream.callbackInfo.thread, INFINITE);
	CloseHandle((HANDLE) m_stream.callbackInfo.thread);

	DsHandle *handle = (DsHandle *) m_stream.apiHandle;
	if (handle) {
		if (handle->buffer[0]) { // the object pointer can be NULL and valid
			LPDIRECTSOUND object = (LPDIRECTSOUND) handle->id[0];
			LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) handle->buffer[0];
			if (buffer) {
				buffer->Stop();
				buffer->Release();
			}
			object->Release();
		}
		if (handle->buffer[1]) {
			LPDIRECTSOUNDCAPTURE object = (LPDIRECTSOUNDCAPTURE) handle->id[1];
			LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) handle->buffer[1];
			if (buffer) {
				buffer->Stop();
				buffer->Release();
			}
			object->Release();
		}
		CloseHandle(handle->condition);
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

void airtaudio::api::Ds::startStream()
{
	verifyStream();
	if (m_stream.state == STREAM_RUNNING) {
		m_errorText = "airtaudio::api::Ds::startStream(): the stream is already running!";
		error(airtaudio::errorWarning);
		return;
	}

	DsHandle *handle = (DsHandle *) m_stream.apiHandle;

	// Increase scheduler frequency on lesser windows (a side-effect of
	// increasing timer accuracy).	On greater windows (Win2K or later),
	// this is already in effect.
	timeBeginPeriod(1); 

	m_buffersRolling = false;
	m_duplexPrerollBytes = 0;

	if (m_stream.mode == DUPLEX) {
		// 0.5 seconds of silence in DUPLEX mode while the devices spin up and synchronize.
		m_duplexPrerollBytes = (int) (0.5 * m_stream.sampleRate * formatBytes(m_stream.deviceFormat[1]) * m_stream.nDeviceChannels[1]);
	}

	HRESULT result = 0;
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {

		LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) handle->buffer[0];
		result = buffer->Play(0, 0, DSBPLAY_LOOPING);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::startStream: error (" << getErrorString(result) << ") starting output buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}

	if (m_stream.mode == INPUT || m_stream.mode == DUPLEX) {

		LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) handle->buffer[1];
		result = buffer->Start(DSCBSTART_LOOPING);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::startStream: error (" << getErrorString(result) << ") starting input buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}
	}

	handle->drainCounter = 0;
	handle->internalDrain = false;
	ResetEvent(handle->condition);
	m_stream.state = STREAM_RUNNING;

 unlock:
	if (FAILED(result)) error(airtaudio::errorSystemError);
}

void airtaudio::api::Ds::stopStream()
{
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Ds::stopStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}

	HRESULT result = 0;
	LPVOID audioPtr;
	DWORD dataLen;
	DsHandle *handle = (DsHandle *) m_stream.apiHandle;
	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {
		if (handle->drainCounter == 0) {
			handle->drainCounter = 2;
			WaitForSingleObject(handle->condition, INFINITE);	// block until signaled
		}

		m_stream.state = STREAM_STOPPED;

		// Stop the buffer and clear memory
		LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) handle->buffer[0];
		result = buffer->Stop();
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::stopStream: error (" << getErrorString(result) << ") stopping output buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}

		// Lock the buffer and clear it so that if we start to play again,
		// we won't have old data playing.
		result = buffer->Lock(0, handle->dsBufferSize[0], &audioPtr, &dataLen, NULL, NULL, 0);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::stopStream: error (" << getErrorString(result) << ") locking output buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}

		// Zero the DS buffer
		ZeroMemory(audioPtr, dataLen);

		// Unlock the DS buffer
		result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::stopStream: error (" << getErrorString(result) << ") unlocking output buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}

		// If we start playing again, we must begin at beginning of buffer.
		handle->bufferPointer[0] = 0;
	}

	if (m_stream.mode == INPUT || m_stream.mode == DUPLEX) {
		LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) handle->buffer[1];
		audioPtr = NULL;
		dataLen = 0;

		m_stream.state = STREAM_STOPPED;

		result = buffer->Stop();
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::stopStream: error (" << getErrorString(result) << ") stopping input buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}

		// Lock the buffer and clear it so that if we start to play again,
		// we won't have old data playing.
		result = buffer->Lock(0, handle->dsBufferSize[1], &audioPtr, &dataLen, NULL, NULL, 0);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::stopStream: error (" << getErrorString(result) << ") locking input buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}

		// Zero the DS buffer
		ZeroMemory(audioPtr, dataLen);

		// Unlock the DS buffer
		result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::stopStream: error (" << getErrorString(result) << ") unlocking input buffer!";
			m_errorText = m_errorStream.str();
			goto unlock;
		}

		// If we start recording again, we must begin at beginning of buffer.
		handle->bufferPointer[1] = 0;
	}

 unlock:
	timeEndPeriod(1); // revert to normal scheduler frequency on lesser windows.
	if (FAILED(result)) error(airtaudio::errorSystemError);
}

void airtaudio::api::Ds::abortStream()
{
	verifyStream();
	if (m_stream.state == STREAM_STOPPED) {
		m_errorText = "airtaudio::api::Ds::abortStream(): the stream is already stopped!";
		error(airtaudio::errorWarning);
		return;
	}

	DsHandle *handle = (DsHandle *) m_stream.apiHandle;
	handle->drainCounter = 2;

	stopStream();
}

void airtaudio::api::Ds::callbackEvent()
{
	if (m_stream.state == STREAM_STOPPED || m_stream.state == STREAM_STOPPING) {
		Sleep(50); // sleep 50 milliseconds
		return;
	}

	if (m_stream.state == STREAM_CLOSED) {
		m_errorText = "airtaudio::api::Ds::callbackEvent(): the stream is closed ... this shouldn't happen!";
		error(airtaudio::errorWarning);
		return;
	}

	CallbackInfo *info = (CallbackInfo *) &m_stream.callbackInfo;
	DsHandle *handle = (DsHandle *) m_stream.apiHandle;

	// Check if we were draining the stream and signal is finished.
	if (handle->drainCounter > m_stream.nBuffers + 2) {

		m_stream.state = STREAM_STOPPING;
		if (handle->internalDrain == false)
			SetEvent(handle->condition);
		else
			stopStream();
		return;
	}

	// Invoke user callback to get fresh output data UNLESS we are
	// draining stream.
	if (handle->drainCounter == 0) {
		airtaudio::AirTAudioCallback callback = (airtaudio::AirTAudioCallback) info->callback;
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
		int32_t cbReturnValue = callback(m_stream.userBuffer[0], m_stream.userBuffer[1],
																	m_stream.bufferSize, streamTime, status, info->userData);
		if (cbReturnValue == 2) {
			m_stream.state = STREAM_STOPPING;
			handle->drainCounter = 2;
			abortStream();
			return;
		}
		else if (cbReturnValue == 1) {
			handle->drainCounter = 1;
			handle->internalDrain = true;
		}
	}

	HRESULT result;
	DWORD currentWritePointer, safeWritePointer;
	DWORD currentReadPointer, safeReadPointer;
	UINT nextWritePointer;

	LPVOID buffer1 = NULL;
	LPVOID buffer2 = NULL;
	DWORD bufferSize1 = 0;
	DWORD bufferSize2 = 0;

	char *buffer;
	long bufferBytes;

	if (m_buffersRolling == false) {
		if (m_stream.mode == DUPLEX) {
			//assert(handle->dsBufferSize[0] == handle->dsBufferSize[1]);

			// It takes a while for the devices to get rolling. As a result,
			// there's no guarantee that the capture and write device pointers
			// will move in lockstep.	Wait here for both devices to start
			// rolling, and then set our buffer pointers accordingly.
			// e.g. Crystal Drivers: the capture buffer starts up 5700 to 9600
			// bytes later than the write buffer.

			// Stub: a serious risk of having a pre-emptive scheduling round
			// take place between the two GetCurrentPosition calls... but I'm
			// really not sure how to solve the problem.	Temporarily boost to
			// Realtime priority, maybe; but I'm not sure what priority the
			// DirectSound service threads run at. We *should* be roughly
			// within a ms or so of correct.

			LPDIRECTSOUNDBUFFER dsWriteBuffer = (LPDIRECTSOUNDBUFFER) handle->buffer[0];
			LPDIRECTSOUNDCAPTUREBUFFER dsCaptureBuffer = (LPDIRECTSOUNDCAPTUREBUFFER) handle->buffer[1];

			DWORD startSafeWritePointer, startSafeReadPointer;

			result = dsWriteBuffer->GetCurrentPosition(NULL, &startSafeWritePointer);
			if (FAILED(result)) {
				m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current write position!";
				m_errorText = m_errorStream.str();
				error(airtaudio::errorSystemError);
				return;
			}
			result = dsCaptureBuffer->GetCurrentPosition(NULL, &startSafeReadPointer);
			if (FAILED(result)) {
				m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current read position!";
				m_errorText = m_errorStream.str();
				error(airtaudio::errorSystemError);
				return;
			}
			while (true) {
				result = dsWriteBuffer->GetCurrentPosition(NULL, &safeWritePointer);
				if (FAILED(result)) {
					m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current write position!";
					m_errorText = m_errorStream.str();
					error(airtaudio::errorSystemError);
					return;
				}
				result = dsCaptureBuffer->GetCurrentPosition(NULL, &safeReadPointer);
				if (FAILED(result)) {
					m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current read position!";
					m_errorText = m_errorStream.str();
					error(airtaudio::errorSystemError);
					return;
				}
				if (safeWritePointer != startSafeWritePointer && safeReadPointer != startSafeReadPointer) break;
				Sleep(1);
			}

			//assert(handle->dsBufferSize[0] == handle->dsBufferSize[1]);

			handle->bufferPointer[0] = safeWritePointer + handle->dsPointerLeadTime[0];
			if (handle->bufferPointer[0] >= handle->dsBufferSize[0]) handle->bufferPointer[0] -= handle->dsBufferSize[0];
			handle->bufferPointer[1] = safeReadPointer;
		}
		else if (m_stream.mode == OUTPUT) {

			// Set the proper nextWritePosition after initial startup.
			LPDIRECTSOUNDBUFFER dsWriteBuffer = (LPDIRECTSOUNDBUFFER) handle->buffer[0];
			result = dsWriteBuffer->GetCurrentPosition(&currentWritePointer, &safeWritePointer);
			if (FAILED(result)) {
				m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current write position!";
				m_errorText = m_errorStream.str();
				error(airtaudio::errorSystemError);
				return;
			}
			handle->bufferPointer[0] = safeWritePointer + handle->dsPointerLeadTime[0];
			if (handle->bufferPointer[0] >= handle->dsBufferSize[0]) handle->bufferPointer[0] -= handle->dsBufferSize[0];
		}

		m_buffersRolling = true;
	}

	if (m_stream.mode == OUTPUT || m_stream.mode == DUPLEX) {
		
		LPDIRECTSOUNDBUFFER dsBuffer = (LPDIRECTSOUNDBUFFER) handle->buffer[0];

		if (handle->drainCounter > 1) { // write zeros to the output stream
			bufferBytes = m_stream.bufferSize * m_stream.nUserChannels[0];
			bufferBytes *= formatBytes(m_stream.userFormat);
			memset(m_stream.userBuffer[0], 0, bufferBytes);
		}

		// Setup parameters and do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[0]) {
			buffer = m_stream.deviceBuffer;
			convertBuffer(buffer, m_stream.userBuffer[0], m_stream.convertInfo[0]);
			bufferBytes = m_stream.bufferSize * m_stream.nDeviceChannels[0];
			bufferBytes *= formatBytes(m_stream.deviceFormat[0]);
		}
		else {
			buffer = m_stream.userBuffer[0];
			bufferBytes = m_stream.bufferSize * m_stream.nUserChannels[0];
			bufferBytes *= formatBytes(m_stream.userFormat);
		}

		// No byte swapping necessary in DirectSound implementation.

		// Ahhh ... windoze.	16-bit data is signed but 8-bit data is
		// unsigned.	So, we need to convert our signed 8-bit data here to
		// unsigned.
		if (m_stream.deviceFormat[0] == RTAUDIO_SINT8)
			for (int32_t i=0; i<bufferBytes; i++) buffer[i] = (unsigned char) (buffer[i] + 128);

		DWORD dsBufferSize = handle->dsBufferSize[0];
		nextWritePointer = handle->bufferPointer[0];

		DWORD endWrite, leadPointer;
		while (true) {
			// Find out where the read and "safe write" pointers are.
			result = dsBuffer->GetCurrentPosition(&currentWritePointer, &safeWritePointer);
			if (FAILED(result)) {
				m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current write position!";
				m_errorText = m_errorStream.str();
				error(airtaudio::errorSystemError);
				return;
			}

			// We will copy our output buffer into the region between
			// safeWritePointer and leadPointer.	If leadPointer is not
			// beyond the next endWrite position, wait until it is.
			leadPointer = safeWritePointer + handle->dsPointerLeadTime[0];
			//std::cout << "safeWritePointer = " << safeWritePointer << ", leadPointer = " << leadPointer << ", nextWritePointer = " << nextWritePointer << std::endl;
			if (leadPointer > dsBufferSize) leadPointer -= dsBufferSize;
			if (leadPointer < nextWritePointer) leadPointer += dsBufferSize; // unwrap offset
			endWrite = nextWritePointer + bufferBytes;

			// Check whether the entire write region is behind the play pointer.
			if (leadPointer >= endWrite) break;

			// If we are here, then we must wait until the leadPointer advances
			// beyond the end of our next write region. We use the
			// Sleep() function to suspend operation until that happens.
			double millis = (endWrite - leadPointer) * 1000.0;
			millis /= (formatBytes(m_stream.deviceFormat[0]) * m_stream.nDeviceChannels[0] * m_stream.sampleRate);
			if (millis < 1.0) millis = 1.0;
			Sleep((DWORD) millis);
		}

		if (dsPointerBetween(nextWritePointer, safeWritePointer, currentWritePointer, dsBufferSize)
				 || dsPointerBetween(endWrite, safeWritePointer, currentWritePointer, dsBufferSize)) { 
			// We've strayed into the forbidden zone ... resync the read pointer.
			handle->xrun[0] = true;
			nextWritePointer = safeWritePointer + handle->dsPointerLeadTime[0] - bufferBytes;
			if (nextWritePointer >= dsBufferSize) nextWritePointer -= dsBufferSize;
			handle->bufferPointer[0] = nextWritePointer;
			endWrite = nextWritePointer + bufferBytes;
		}

		// Lock free space in the buffer
		result = dsBuffer->Lock(nextWritePointer, bufferBytes, &buffer1,
														 &bufferSize1, &buffer2, &bufferSize2, 0);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") locking buffer during playback!";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorSystemError);
			return;
		}

		// Copy our buffer into the DS buffer
		CopyMemory(buffer1, buffer, bufferSize1);
		if (buffer2 != NULL) CopyMemory(buffer2, buffer+bufferSize1, bufferSize2);

		// Update our buffer offset and unlock sound buffer
		dsBuffer->Unlock(buffer1, bufferSize1, buffer2, bufferSize2);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") unlocking buffer during playback!";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorSystemError);
			return;
		}
		nextWritePointer = (nextWritePointer + bufferSize1 + bufferSize2) % dsBufferSize;
		handle->bufferPointer[0] = nextWritePointer;

		if (handle->drainCounter) {
			handle->drainCounter++;
			goto unlock;
		}
	}

	if (m_stream.mode == INPUT || m_stream.mode == DUPLEX) {

		// Setup parameters.
		if (m_stream.doConvertBuffer[1]) {
			buffer = m_stream.deviceBuffer;
			bufferBytes = m_stream.bufferSize * m_stream.nDeviceChannels[1];
			bufferBytes *= formatBytes(m_stream.deviceFormat[1]);
		}
		else {
			buffer = m_stream.userBuffer[1];
			bufferBytes = m_stream.bufferSize * m_stream.nUserChannels[1];
			bufferBytes *= formatBytes(m_stream.userFormat);
		}

		LPDIRECTSOUNDCAPTUREBUFFER dsBuffer = (LPDIRECTSOUNDCAPTUREBUFFER) handle->buffer[1];
		long nextReadPointer = handle->bufferPointer[1];
		DWORD dsBufferSize = handle->dsBufferSize[1];

		// Find out where the write and "safe read" pointers are.
		result = dsBuffer->GetCurrentPosition(&currentReadPointer, &safeReadPointer);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current read position!";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorSystemError);
			return;
		}

		if (safeReadPointer < (DWORD)nextReadPointer) safeReadPointer += dsBufferSize; // unwrap offset
		DWORD endRead = nextReadPointer + bufferBytes;

		// Handling depends on whether we are INPUT or DUPLEX. 
		// If we're in INPUT mode then waiting is a good thing. If we're in DUPLEX mode,
		// then a wait here will drag the write pointers into the forbidden zone.
		// 
		// In DUPLEX mode, rather than wait, we will back off the read pointer until 
		// it's in a safe position. This causes dropouts, but it seems to be the only 
		// practical way to sync up the read and write pointers reliably, given the 
		// the very complex relationship between phase and increment of the read and write 
		// pointers.
		//
		// In order to minimize audible dropouts in DUPLEX mode, we will
		// provide a pre-roll period of 0.5 seconds in which we return
		// zeros from the read buffer while the pointers sync up.

		if (m_stream.mode == DUPLEX) {
			if (safeReadPointer < endRead) {
				if (m_duplexPrerollBytes <= 0) {
					// Pre-roll time over. Be more agressive.
					int32_t adjustment = endRead-safeReadPointer;

					handle->xrun[1] = true;
					// Two cases:
					//	 - large adjustments: we've probably run out of CPU cycles, so just resync exactly,
					//		 and perform fine adjustments later.
					//	 - small adjustments: back off by twice as much.
					if (adjustment >= 2*bufferBytes)
						nextReadPointer = safeReadPointer-2*bufferBytes;
					else
						nextReadPointer = safeReadPointer-bufferBytes-adjustment;

					if (nextReadPointer < 0) nextReadPointer += dsBufferSize;

				}
				else {
					// In pre=roll time. Just do it.
					nextReadPointer = safeReadPointer - bufferBytes;
					while (nextReadPointer < 0) nextReadPointer += dsBufferSize;
				}
				endRead = nextReadPointer + bufferBytes;
			}
		}
		else { // mode == INPUT
			while (safeReadPointer < endRead && m_stream.callbackInfo.isRunning) {
				// See comments for playback.
				double millis = (endRead - safeReadPointer) * 1000.0;
				millis /= (formatBytes(m_stream.deviceFormat[1]) * m_stream.nDeviceChannels[1] * m_stream.sampleRate);
				if (millis < 1.0) millis = 1.0;
				Sleep((DWORD) millis);

				// Wake up and find out where we are now.
				result = dsBuffer->GetCurrentPosition(&currentReadPointer, &safeReadPointer);
				if (FAILED(result)) {
					m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") getting current read position!";
					m_errorText = m_errorStream.str();
					error(airtaudio::errorSystemError);
					return;
				}
			
				if (safeReadPointer < (DWORD)nextReadPointer) safeReadPointer += dsBufferSize; // unwrap offset
			}
		}

		// Lock free space in the buffer
		result = dsBuffer->Lock(nextReadPointer, bufferBytes, &buffer1,
														 &bufferSize1, &buffer2, &bufferSize2, 0);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") locking capture buffer!";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorSystemError);
			return;
		}

		if (m_duplexPrerollBytes <= 0) {
			// Copy our buffer into the DS buffer
			CopyMemory(buffer, buffer1, bufferSize1);
			if (buffer2 != NULL) CopyMemory(buffer+bufferSize1, buffer2, bufferSize2);
		}
		else {
			memset(buffer, 0, bufferSize1);
			if (buffer2 != NULL) memset(buffer + bufferSize1, 0, bufferSize2);
			m_duplexPrerollBytes -= bufferSize1 + bufferSize2;
		}

		// Update our buffer offset and unlock sound buffer
		nextReadPointer = (nextReadPointer + bufferSize1 + bufferSize2) % dsBufferSize;
		dsBuffer->Unlock(buffer1, bufferSize1, buffer2, bufferSize2);
		if (FAILED(result)) {
			m_errorStream << "airtaudio::api::Ds::callbackEvent: error (" << getErrorString(result) << ") unlocking capture buffer!";
			m_errorText = m_errorStream.str();
			error(airtaudio::errorSystemError);
			return;
		}
		handle->bufferPointer[1] = nextReadPointer;

		// No byte swapping necessary in DirectSound implementation.

		// If necessary, convert 8-bit data from unsigned to signed.
		if (m_stream.deviceFormat[1] == RTAUDIO_SINT8)
			for (int32_t j=0; j<bufferBytes; j++) buffer[j] = (signed char) (buffer[j] - 128);

		// Do buffer conversion if necessary.
		if (m_stream.doConvertBuffer[1])
			convertBuffer(m_stream.userBuffer[1], m_stream.deviceBuffer, m_stream.convertInfo[1]);
	}

 unlock:
	RtApi::tickStreamTime();
}

// Definitions for utility functions and callbacks
// specific to the DirectSound implementation.

static unsigned __stdcall callbackHandler(void *ptr)
{
	CallbackInfo *info = (CallbackInfo *) ptr;
	RtApiDs *object = (RtApiDs *) info->object;
	bool* isRunning = &info->isRunning;

	while (*isRunning == true) {
		object->callbackEvent();
	}

	_endthreadex(0);
	return 0;
}

#include "tchar.h"

static std::string convertTChar(LPCTSTR name)
{
#if defined(UNICODE) || defined(_UNICODE)
	int32_t length = WideCharToMultiByte(CP_UTF8, 0, name, -1, NULL, 0, NULL, NULL);
	std::string s(length-1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, name, -1, &s[0], length, NULL, NULL);
#else
	std::string s(name);
#endif

	return s;
}

static BOOL CALLBACK deviceQueryCallback(LPGUID lpguid,
																					LPCTSTR description,
																					LPCTSTR module,
																					LPVOID lpContext)
{
	struct DsProbeData& probeInfo = *(struct DsProbeData*) lpContext;
	std::vector<struct DsDevice>& dsDevices = *probeInfo.dsDevices;

	HRESULT hr;
	bool validDevice = false;
	if (probeInfo.isInput == true) {
		DSCCAPS caps;
		LPDIRECTSOUNDCAPTURE object;

		hr = DirectSoundCaptureCreate(lpguid, &object,	 NULL);
		if (hr != DS_OK) return TRUE;

		caps.dwSize = sizeof(caps);
		hr = object->GetCaps(&caps);
		if (hr == DS_OK) {
			if (caps.dwChannels > 0 && caps.dwFormats > 0)
				validDevice = true;
		}
		object->Release();
	}
	else {
		DSCAPS caps;
		LPDIRECTSOUND object;
		hr = DirectSoundCreate(lpguid, &object,	 NULL);
		if (hr != DS_OK) return TRUE;

		caps.dwSize = sizeof(caps);
		hr = object->GetCaps(&caps);
		if (hr == DS_OK) {
			if (caps.dwFlags & DSCAPS_PRIMARYMONO || caps.dwFlags & DSCAPS_PRIMARYSTEREO)
				validDevice = true;
		}
		object->Release();
	}

	// If good device, then save its name and guid.
	std::string name = convertTChar(description);
	//if (name == "Primary Sound Driver" || name == "Primary Sound Capture Driver")
	if (lpguid == NULL)
		name = "Default Device";
	if (validDevice) {
		for (uint32_t i=0; i<dsDevices.size(); i++) {
			if (dsDevices[i].name == name) {
				dsDevices[i].found = true;
				if (probeInfo.isInput) {
					dsDevices[i].id[1] = lpguid;
					dsDevices[i].validId[1] = true;
				}
				else {
					dsDevices[i].id[0] = lpguid;
					dsDevices[i].validId[0] = true;
				}
				return TRUE;
			}
		}

		DsDevice device;
		device.name = name;
		device.found = true;
		if (probeInfo.isInput) {
			device.id[1] = lpguid;
			device.validId[1] = true;
		}
		else {
			device.id[0] = lpguid;
			device.validId[0] = true;
		}
		dsDevices.push_back(device);
	}

	return TRUE;
}

static const char* getErrorString(int32_t code)
{
	switch (code) {

	case DSERR_ALLOCATED:
		return "Already allocated";

	case DSERR_CONTROLUNAVAIL:
		return "Control unavailable";

	case DSERR_INVALIDPARAM:
		return "Invalid parameter";

	case DSERR_INVALIDCALL:
		return "Invalid call";

	case DSERR_GENERIC:
		return "Generic error";

	case DSERR_PRIOLEVELNEEDED:
		return "Priority level needed";

	case DSERR_OUTOFMEMORY:
		return "Out of memory";

	case DSERR_BADFORMAT:
		return "The sample rate or the channel format is not supported";

	case DSERR_UNSUPPORTED:
		return "Not supported";

	case DSERR_NODRIVER:
		return "No driver";

	case DSERR_ALREADYINITIALIZED:
		return "Already initialized";

	case DSERR_NOAGGREGATION:
		return "No aggregation";

	case DSERR_BUFFERLOST:
		return "Buffer lost";

	case DSERR_OTHERAPPHASPRIO:
		return "Another application already has priority";

	case DSERR_UNINITIALIZED:
		return "Uninitialized";

	default:
		return "DirectSound unknown error";
	}
}
//******************** End of __WINDOWS_DS__ *********************//
#endif
