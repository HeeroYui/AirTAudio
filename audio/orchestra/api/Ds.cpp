/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

// Windows DirectSound API
#if defined(ORCHESTRA_BUILD_DS)
#include <audio/orchestra/Interface.hpp>
#include <audio/orchestra/debug.hpp>
#include <ethread/tools.hpp>
#include <audio/orchestra/api/Ds.hpp>

ememory::SharedPtr<audio::orchestra::Api> audio::orchestra::api::Ds::create() {
	return ememory::SharedPtr<audio::orchestra::api::Ds>(new audio::orchestra::api::Ds());
}


// Modified by Robin Davies, October 2005
// - Improvements to DirectX pointer chasing. 
// - Bug fix for non-power-of-two Asio granularity used by Edirol PCR-A30.
// - Auto-call CoInitialize for DSOUND and ASIO platforms.
// Various revisions for RtAudio 4.0 by Gary Scavone, April 2007
// Changed device query structure for RtAudio 4.0.7, January 2010

#include <dsound.h>
#include <cassert>
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

static inline DWORD dsPointerBetween(DWORD _pointer, DWORD _laterPointer, DWORD _earlierPointer, DWORD _bufferSize) {
	if (_pointer > _bufferSize) {
		_pointer -= _bufferSize;
	}
	if (_laterPointer < _earlierPointer) {
		_laterPointer += _bufferSize;
	}
	if (_pointer < _earlierPointer) {
		_pointer += _bufferSize;
	}
	return _pointer >= _earlierPointer && _pointer < _laterPointer;
}

class DsDevice {
	public:
		LPGUID id;
		bool input;
		etk::String name;
		DsDevice() :
		  id(0),
		  input(false) {
			
		}
};

namespace audio {
	namespace orchestra {
		namespace api {
			class DsPrivate {
				public:
					ememory::SharedPtr<std::thread> thread;
					bool threadRunning;
					uint32_t drainCounter; // Tracks callback counts when draining
					bool internalDrain; // Indicates if stop is initiated from callback or not.
					void *id[2];
					void *buffer[2];
					bool xrun[2];
					UINT bufferPointer[2];
					DWORD dsBufferSize[2];
					DWORD dsPointerLeadTime[2]; // the number of bytes ahead of the safe pointer to lead by.
					HANDLE condition;
					etk::Vector<DsDevice> dsDevices;
					DsPrivate() :
					  threadRunning(false),
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
		}
	}
}


static const char* getErrorString(int32_t _code);

struct DsProbeData {
	bool isInput;
	etk::Vector<DsDevice>* dsDevices;
};

audio::orchestra::api::Ds::Ds() :
  m_private(new audio::orchestra::api::DsPrivate()) {
	// Dsound will run both-threaded. If CoInitialize fails, then just
	// accept whatever the mainline chose for a threading model.
	m_coInitialized = false;
	HRESULT hr = CoInitialize(nullptr);
	if (!FAILED(hr)) {
		m_coInitialized = true;
	}
}

audio::orchestra::api::Ds::~Ds() {
	if (m_coInitialized) {
		CoUninitialize(); // balanced call.
	}
	if (m_state != audio::orchestra::state::closed) {
		closeStream();
	}
}


#include "tchar.h"
static etk::String convertTChar(LPCTSTR _name) {
#if defined(UNICODE) || defined(_UNICODE)
	int32_t length = WideCharToMultiByte(CP_UTF8, 0, _name, -1, nullptr, 0, nullptr, nullptr);
	etk::String s(length-1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, _name, -1, &s[0], length, nullptr, nullptr);
#else
	etk::String s(_name);
#endif
	return s;
}

static BOOL CALLBACK deviceQueryCallback(LPGUID _lpguid,
                                         LPCTSTR _description,
                                         LPCTSTR _module,
                                         LPVOID _lpContext) {
	struct DsProbeData& probeInfo = *(struct DsProbeData*) _lpContext;
	etk::Vector<DsDevice>& dsDevices = *probeInfo.dsDevices;
	HRESULT hr;
	bool validDevice = false;
	if (probeInfo.isInput == true) {
		DSCCAPS caps;
		LPDIRECTSOUNDCAPTURE object;
		hr = DirectSoundCaptureCreate(_lpguid, &object, nullptr);
		if (hr != DS_OK) {
			return TRUE;
		}
		caps.dwSize = sizeof(caps);
		hr = object->GetCaps(&caps);
		if (hr == DS_OK) {
			if (caps.dwChannels > 0 && caps.dwFormats > 0) {
				validDevice = true;
			}
		}
		object->Release();
	} else {
		DSCAPS caps;
		LPDIRECTSOUND object;
		hr = DirectSoundCreate(_lpguid, &object, nullptr);
		if (hr != DS_OK) {
			return TRUE;
		}
		caps.dwSize = sizeof(caps);
		hr = object->GetCaps(&caps);
		if (hr == DS_OK) {
			if (    caps.dwFlags & DSCAPS_PRIMARYMONO
			     || caps.dwFlags & DSCAPS_PRIMARYSTEREO) {
				validDevice = true;
			}
		}
		object->Release();
	}
	if (validDevice == false) {
		return TRUE;
	}
	// If good device, then save its name and guid.
	etk::String name = convertTChar(_description);
	//if (name == "Primary Sound Driver" || name == "Primary Sound Capture Driver")
	if (_lpguid == nullptr) {
		name = "Default Device";
	}
	DsDevice device;
	device.name = name;
	device.input = probeInfo.isInput;
	device.id = _lpguid;
	dsDevices.pushBack(device);
	return TRUE;
}

uint32_t audio::orchestra::api::Ds::getDeviceCount() {
	if (m_private->dsDevices.size()>0) {
		return m_private->dsDevices.size();
	}
	// Query DirectSound devices.
	struct DsProbeData probeInfo;
	probeInfo.isInput = false;
	probeInfo.dsDevices = &m_private->dsDevices;
	HRESULT result = DirectSoundEnumerate((LPDSENUMCALLBACK) deviceQueryCallback, &probeInfo);
	if (FAILED(result)) {
		ATA_ERROR(getErrorString(result) << ": enumerating output devices!");
		return 0;
	}
	// Query DirectSoundCapture devices.
	probeInfo.isInput = true;
	result = DirectSoundCaptureEnumerate((LPDSENUMCALLBACK) deviceQueryCallback, &probeInfo);
	if (FAILED(result)) {
		ATA_ERROR(getErrorString(result) << ": enumerating input devices!");
		return 0;
	}
	return m_private->dsDevices.size();
}

audio::orchestra::DeviceInfo audio::orchestra::api::Ds::getDeviceInfo(uint32_t _device) {
	audio::orchestra::DeviceInfo info;
	if (_device >= m_private->dsDevices.size()) {
		ATA_ERROR("device ID is invalid!");
		return info;
	}
	HRESULT result;
	if (m_private->dsDevices[_device].input == false) {
		info.input = true;
		LPDIRECTSOUND output;
		DSCAPS outCaps;
		result = DirectSoundCreate(m_private->dsDevices[_device].id, &output, nullptr);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": opening output device (" << m_private->dsDevices[_device].name << ")!");
			info.clear();
			return info;
		}
		outCaps.dwSize = sizeof(outCaps);
		result = output->GetCaps(&outCaps);
		if (FAILED(result)) {
			output->Release();
			ATA_ERROR(getErrorString(result) << ": getting capabilities!");
			info.clear();
			return info;
		}
		// Get output channel information.
		if (outCaps.dwFlags & DSCAPS_PRIMARYSTEREO) {
			info.channels.pushBack(audio::channel_unknow);
			info.channels.pushBack(audio::channel_unknow);
		} else {
			info.channels.pushBack(audio::channel_unknow);
		}
		// Get sample rate information.
		for (auto &it : audio::orchestra::genericSampleRate()) {
			if (    it >= outCaps.dwMinSecondarySampleRate
			     && it <= outCaps.dwMaxSecondarySampleRate) {
				info.sampleRates.pushBack(it);
			}
		}
		// Get format information.
		if (outCaps.dwFlags & DSCAPS_PRIMARY16BIT) {
			info.nativeFormats.pushBack(audio::format_int16);
		}
		if (outCaps.dwFlags & DSCAPS_PRIMARY8BIT) {
			info.nativeFormats.pushBack(audio::format_int8);
		}
		output->Release();
		info.name = m_private->dsDevices[_device].name;
		info.isCorrect = true;
		return info;
	} else {
		LPDIRECTSOUNDCAPTURE input;
		result = DirectSoundCaptureCreate(m_private->dsDevices[_device].id, &input, nullptr);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": opening input device (" << m_private->dsDevices[_device].name << ")!");
			info.clear();
			return info;
		}
		DSCCAPS inCaps;
		inCaps.dwSize = sizeof(inCaps);
		result = input->GetCaps(&inCaps);
		if (FAILED(result)) {
			input->Release();
			ATA_ERROR(getErrorString(result) << ": getting object capabilities (" << m_private->dsDevices[_device].name << ")!");
			info.clear();
			return info;
		}
		// Get input channel information.
		for (int32_t iii=0; iii<inCaps.dwChannels; ++iii) {
			info.channels.pushBack(audio::channel_unknow);
		}
		// Get sample rate and format information.
		etk::Vector<uint32_t> rates;
		if (inCaps.dwChannels >= 2) {
			if (    (inCaps.dwFormats & WAVE_FORMAT_1S16)
			     || (inCaps.dwFormats & WAVE_FORMAT_2S16)
			     || (inCaps.dwFormats & WAVE_FORMAT_4S16)
			     || (inCaps.dwFormats & WAVE_FORMAT_96S16) ) {
				info.nativeFormats.pushBack(audio::format_int16);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_1S08)
			     || (inCaps.dwFormats & WAVE_FORMAT_2S08)
			     || (inCaps.dwFormats & WAVE_FORMAT_4S08)
			     || (inCaps.dwFormats & WAVE_FORMAT_96S08) ) {
				info.nativeFormats.pushBack(audio::format_int8);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_1S16)
			     || (inCaps.dwFormats & WAVE_FORMAT_1S08) ){
				rates.pushBack(11025);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_2S16)
			     || (inCaps.dwFormats & WAVE_FORMAT_2S08) ){
				rates.pushBack(22050);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_4S16)
			     || (inCaps.dwFormats & WAVE_FORMAT_4S08) ){
				rates.pushBack(44100);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_96S16)
			     || (inCaps.dwFormats & WAVE_FORMAT_96S08) ){
				rates.pushBack(96000);
			}
		} else if (inCaps.dwChannels == 1) {
			if (    (inCaps.dwFormats & WAVE_FORMAT_1M16)
			     || (inCaps.dwFormats & WAVE_FORMAT_2M16)
			     || (inCaps.dwFormats & WAVE_FORMAT_4M16)
			     || (inCaps.dwFormats & WAVE_FORMAT_96M16) ) {
				info.nativeFormats.pushBack(audio::format_int16);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_1M08)
			     || (inCaps.dwFormats & WAVE_FORMAT_2M08)
			     || (inCaps.dwFormats & WAVE_FORMAT_4M08)
			     || (inCaps.dwFormats & WAVE_FORMAT_96M08) ) {
				info.nativeFormats.pushBack(audio::format_int8);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_1M16)
			     || (inCaps.dwFormats & WAVE_FORMAT_1M08) ){
				rates.pushBack(11025);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_2M16)
			     || (inCaps.dwFormats & WAVE_FORMAT_2M08) ){
				rates.pushBack(22050);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_4M16)
			     || (inCaps.dwFormats & WAVE_FORMAT_4M08) ){
				rates.pushBack(44100);
			}
			if (    (inCaps.dwFormats & WAVE_FORMAT_96M16)
			     || (inCaps.dwFormats & WAVE_FORMAT_96M08) ){
				rates.pushBack(96000);
			}
		} else {
			// technically, this would be an error
			info.channels.clear();
		}
		input->Release();
		if (info.channels.size() == 0) {
			info.clear();
			return info;
		}
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
			if (found == false) {
				info.sampleRates.pushBack(rates[i]);
			}
		}
		std::sort(info.sampleRates.begin(), info.sampleRates.end());
		// Copy name and return.
		info.name = m_private->dsDevices[_device].name;
		info.isCorrect = true;
		return info;
	}
	info.clear();
	return info;
}

bool audio::orchestra::api::Ds::open(uint32_t _device,
                                     enum audio::orchestra::mode _mode,
                                     uint32_t _channels,
                                     uint32_t _firstChannel,
                                     uint32_t _sampleRate,
                                     enum audio::format _format,
                                     uint32_t *_bufferSize,
                                     const audio::orchestra::StreamOptions& _options) {
	if (_channels + _firstChannel > 2) {
		ATA_ERROR("DirectSound does not support more than 2 channels per device.");
		return false;
	}
	uint32_t nDevices = m_private->dsDevices.size();
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
	/*
	nBuffers = _options.numberOfBuffers;
	*/
	if (_options.flags.m_minimizeLatency == true) {
		nBuffers = 2;
	}
	if (nBuffers < 2) {
		nBuffers = 3;
	}
	// Check the lower range of the user-specified buffer size and set
	// (arbitrarily) to a lower bound of 32.
	if (*_bufferSize < 32) {
		*_bufferSize = 32;
	}
	// Create the wave format structure.	The data format setting will
	// be determined later.
	WAVEFORMATEX waveFormat;
	ZeroMemory(&waveFormat, sizeof(WAVEFORMATEX));
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = _channels + _firstChannel;
	waveFormat.nSamplesPerSec = (uint64_t) _sampleRate;
	// Determine the device buffer size. By default, we'll use the value
	// defined above (32K), but we will grow it to make allowances for
	// very large software buffer sizes.
	DWORD dsBufferSize = MINIMUM_DEVICE_BUFFER_SIZE;
	DWORD dsPointerLeadTime = 0;
	void *ohandle = 0, *bhandle = 0;
	HRESULT result;
	if (_mode == audio::orchestra::mode_output) {
		LPDIRECTSOUND output;
		result = DirectSoundCreate(m_private->dsDevices[_device].id, &output, nullptr);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": opening output device (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		DSCAPS outCaps;
		outCaps.dwSize = sizeof(outCaps);
		result = output->GetCaps(&outCaps);
		if (FAILED(result)) {
			output->Release();
			ATA_ERROR(getErrorString(result) << ": getting capabilities (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		// Check channel information.
		if (_channels + _firstChannel == 2 && !(outCaps.dwFlags & DSCAPS_PRIMARYSTEREO)) {
			ATA_ERROR("the output device (" << m_private->dsDevices[_device].name << ") does not support stereo playback.");
			return false;
		}
		// Check format information.	Use 16-bit format unless not
		// supported or user requests 8-bit.
		if (    outCaps.dwFlags & DSCAPS_PRIMARY16BIT
		     && !(    _format == audio::format_int8
		           && outCaps.dwFlags & DSCAPS_PRIMARY8BIT)) {
			waveFormat.wBitsPerSample = 16;
			m_deviceFormat[modeToIdTable(_mode)] = audio::format_int16;
		} else {
			waveFormat.wBitsPerSample = 8;
			m_deviceFormat[modeToIdTable(_mode)] = audio::format_int8;
		}
		m_userFormat = _format;
		// Update wave format structure and buffer information.
		waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		dsPointerLeadTime = nBuffers * (*_bufferSize) * (waveFormat.wBitsPerSample / 8) * _channels;
		// If the user wants an even bigger buffer, increase the device buffer size accordingly.
		while (dsPointerLeadTime * 2U > dsBufferSize) {
			dsBufferSize *= 2;
		}
		// Set cooperative level to DSSCL_EXCLUSIVE ... sound stops when window focus changes.
		// result = output->SetCooperativeLevel(hWnd, DSSCL_EXCLUSIVE);
		// Set cooperative level to DSSCL_PRIORITY ... sound remains when window focus changes.
		result = output->SetCooperativeLevel(hWnd, DSSCL_PRIORITY);
		if (FAILED(result)) {
			output->Release();
			ATA_ERROR(getErrorString(result) << ": setting cooperative level (" << m_private->dsDevices[_device].name << ")!");
			return false;
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
		result = output->CreateSoundBuffer(&bufferDescription, &buffer, nullptr);
		if (FAILED(result)) {
			output->Release();
			ATA_ERROR(getErrorString(result) << ": accessing primary buffer (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		// Set the primary DS buffer sound format.
		result = buffer->SetFormat(&waveFormat);
		if (FAILED(result)) {
			output->Release();
			ATA_ERROR(getErrorString(result) << ": setting primary buffer format (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		// Setup the secondary DS buffer description.
		ZeroMemory(&bufferDescription, sizeof(DSBUFFERDESC));
		bufferDescription.dwSize = sizeof(DSBUFFERDESC);
		bufferDescription.dwFlags = (   DSBCAPS_STICKYFOCUS
		                              | DSBCAPS_GLOBALFOCUS
		                              | DSBCAPS_GETCURRENTPOSITION2
		                              | DSBCAPS_LOCHARDWARE);	// Force hardware mixing
		bufferDescription.dwBufferBytes = dsBufferSize;
		bufferDescription.lpwfxFormat = &waveFormat;
		// Try to create the secondary DS buffer.	If that doesn't work,
		// try to use software mixing.	Otherwise, there's a problem.
		result = output->CreateSoundBuffer(&bufferDescription, &buffer, nullptr);
		if (FAILED(result)) {
			bufferDescription.dwFlags = (   DSBCAPS_STICKYFOCUS
			                              | DSBCAPS_GLOBALFOCUS
			                              | DSBCAPS_GETCURRENTPOSITION2
			                              | DSBCAPS_LOCSOFTWARE);	// Force software mixing
			result = output->CreateSoundBuffer(&bufferDescription, &buffer, nullptr);
			if (FAILED(result)) {
				output->Release();
				ATA_ERROR(getErrorString(result) << ": creating secondary buffer (" << m_private->dsDevices[_device].name << ")!");
				return false;
			}
		}
		// Get the buffer size ... might be different from what we specified.
		DSBCAPS dsbcaps;
		dsbcaps.dwSize = sizeof(DSBCAPS);
		result = buffer->GetCaps(&dsbcaps);
		if (FAILED(result)) {
			output->Release();
			buffer->Release();
			ATA_ERROR(getErrorString(result) << ": getting buffer settings (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		dsBufferSize = dsbcaps.dwBufferBytes;
		// Lock the DS buffer
		LPVOID audioPtr;
		DWORD dataLen;
		result = buffer->Lock(0, dsBufferSize, &audioPtr, &dataLen, nullptr, nullptr, 0);
		if (FAILED(result)) {
			output->Release();
			buffer->Release();
			ATA_ERROR(getErrorString(result) << ": locking buffer (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		// Zero the DS buffer
		ZeroMemory(audioPtr, dataLen);
		// Unlock the DS buffer
		result = buffer->Unlock(audioPtr, dataLen, nullptr, 0);
		if (FAILED(result)) {
			output->Release();
			buffer->Release();
			ATA_ERROR(getErrorString(result) << ": unlocking buffer (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		ohandle = (void *) output;
		bhandle = (void *) buffer;
	}
	if (_mode == audio::orchestra::mode_input) {
		LPDIRECTSOUNDCAPTURE input;
		result = DirectSoundCaptureCreate(m_private->dsDevices[_device].id, &input, nullptr);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": opening input device (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		DSCCAPS inCaps;
		inCaps.dwSize = sizeof(inCaps);
		result = input->GetCaps(&inCaps);
		if (FAILED(result)) {
			input->Release();
			ATA_ERROR(getErrorString(result) << ": getting input capabilities (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		// Check channel information.
		if (inCaps.dwChannels < _channels + _firstChannel) {
			ATA_ERROR("the input device does not support requested input channels.");
			return false;
		}
		// Check format information.	Use 16-bit format unless user
		// requests 8-bit.
		DWORD deviceFormats;
		if (_channels + _firstChannel == 2) {
			deviceFormats = WAVE_FORMAT_1S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4S08 | WAVE_FORMAT_96S08;
			if (_format == audio::format_int8 && inCaps.dwFormats & deviceFormats) {
				waveFormat.wBitsPerSample = 8;
				m_deviceFormat[modeToIdTable(_mode)] = audio::format_int8;
			} else { // assume 16-bit is supported
				waveFormat.wBitsPerSample = 16;
				m_deviceFormat[modeToIdTable(_mode)] = audio::format_int16;
			}
		} else { // channel == 1
			deviceFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_4M08 | WAVE_FORMAT_96M08;
			if (_format == audio::format_int8 && inCaps.dwFormats & deviceFormats) {
				waveFormat.wBitsPerSample = 8;
				m_deviceFormat[modeToIdTable(_mode)] = audio::format_int8;
			}
			else { // assume 16-bit is supported
				waveFormat.wBitsPerSample = 16;
				m_deviceFormat[modeToIdTable(_mode)] = audio::format_int16;
			}
		}
		m_userFormat = _format;
		// Update wave format structure and buffer information.
		waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
		dsPointerLeadTime = nBuffers * (*_bufferSize) * (waveFormat.wBitsPerSample / 8) * _channels;
		// If the user wants an even bigger buffer, increase the device buffer size accordingly.
		while (dsPointerLeadTime * 2U > dsBufferSize) {
			dsBufferSize *= 2;
		}
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
		result = input->CreateCaptureBuffer(&bufferDescription, &buffer, nullptr);
		if (FAILED(result)) {
			input->Release();
			ATA_ERROR(getErrorString(result) << ": creating input buffer (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		// Get the buffer size ... might be different from what we specified.
		DSCBCAPS dscbcaps;
		dscbcaps.dwSize = sizeof(DSCBCAPS);
		result = buffer->GetCaps(&dscbcaps);
		if (FAILED(result)) {
			input->Release();
			buffer->Release();
			ATA_ERROR(getErrorString(result) << ": getting buffer settings (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		dsBufferSize = dscbcaps.dwBufferBytes;
		// NOTE: We could have a problem here if this is a duplex stream
		// and the play and capture hardware buffer sizes are different
		// (I'm actually not sure if that is a problem or not).
		// Currently, we are not verifying that.
		// Lock the capture buffer
		LPVOID audioPtr;
		DWORD dataLen;
		result = buffer->Lock(0, dsBufferSize, &audioPtr, &dataLen, nullptr, nullptr, 0);
		if (FAILED(result)) {
			input->Release();
			buffer->Release();
			ATA_ERROR(getErrorString(result) << ": locking input buffer (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		// Zero the buffer
		ZeroMemory(audioPtr, dataLen);
		// Unlock the buffer
		result = buffer->Unlock(audioPtr, dataLen, nullptr, 0);
		if (FAILED(result)) {
			input->Release();
			buffer->Release();
			ATA_ERROR(getErrorString(result) << ": unlocking input buffer (" << m_private->dsDevices[_device].name << ")!");
			return false;
		}
		ohandle = (void *) input;
		bhandle = (void *) buffer;
	}
	// Set various stream parameters
	m_nDeviceChannels[modeToIdTable(_mode)] = _channels + _firstChannel;
	m_nUserChannels[modeToIdTable(_mode)] = _channels;
	m_bufferSize = *_bufferSize;
	m_channelOffset[modeToIdTable(_mode)] = _firstChannel;
	m_deviceInterleaved[modeToIdTable(_mode)] = true;
	// Set flag for buffer conversion
	m_doConvertBuffer[modeToIdTable(_mode)] = false;
	if (m_nUserChannels[modeToIdTable(_mode)] != m_nDeviceChannels[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (m_userFormat != m_deviceFormat[modeToIdTable(_mode)]) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	if (    m_deviceInterleaved[modeToIdTable(_mode)] == false
	     && m_nUserChannels[modeToIdTable(_mode)] > 1) {
		m_doConvertBuffer[modeToIdTable(_mode)] = true;
	}
	// Allocate necessary internal buffers
	long bufferBytes = m_nUserChannels[modeToIdTable(_mode)] * *_bufferSize * audio::getFormatBytes(m_userFormat);
	m_userBuffer[modeToIdTable(_mode)].resize(bufferBytes, 0);
	if (m_userBuffer[modeToIdTable(_mode)].size() == 0) {
		ATA_ERROR("error allocating user buffer memory.");
		goto error;
	}
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		bool makeBuffer = true;
		bufferBytes = m_nDeviceChannels[modeToIdTable(_mode)] * audio::getFormatBytes(m_deviceFormat[modeToIdTable(_mode)]);
		if (_mode == audio::orchestra::mode_input) {
			if (m_mode == audio::orchestra::mode_output && m_deviceBuffer) {
				uint64_t bytesOut = m_nDeviceChannels[0] * audio::getFormatBytes(m_deviceFormat[0]);
				if (bufferBytes <= (long) bytesOut) {
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
	// Create a manual-reset event.
	m_private->condition = CreateEvent(nullptr,  // no security
	                                TRUE,  // manual-reset
	                                FALSE, // non-signaled initially
	                                nullptr); // unnamed
	m_private->id[modeToIdTable(_mode)] = ohandle;
	m_private->buffer[modeToIdTable(_mode)] = bhandle;
	m_private->dsBufferSize[modeToIdTable(_mode)] = dsBufferSize;
	m_private->dsPointerLeadTime[modeToIdTable(_mode)] = dsPointerLeadTime;
	m_device[modeToIdTable(_mode)] = _device;
	m_state = audio::orchestra::state::stopped;
	if (    m_mode == audio::orchestra::mode_output
	     && _mode == audio::orchestra::mode_input) {
		// We had already set up an output stream.
		m_mode = audio::orchestra::mode_duplex;
	} else {
		m_mode = _mode;
	}
	m_nBuffers = nBuffers;
	m_sampleRate = _sampleRate;
	// Setup the buffer conversion information structure.
	if (m_doConvertBuffer[modeToIdTable(_mode)]) {
		setConvertInfo(_mode, _firstChannel);
	}
	// Setup the callback thread.
	if (m_private->threadRunning == false) {
		m_private->threadRunning = true;
		ememory::SharedPtr<std::thread> tmpThread(new std::thread(&audio::orchestra::api::Ds::dsCallbackEvent, this));
		m_private->thread =	etk::move(tmpThread);
		if (m_private->thread == nullptr) {
			ATA_ERROR("error creating callback thread!");
			goto error;
		}
		// Boost DS thread priority
		ethread::setPriority(*m_private->thread, -6);
	}
	return true;
error:
	if (m_private->buffer[0]) {
		// the object pointer can be nullptr and valid
		LPDIRECTSOUND object = (LPDIRECTSOUND) m_private->id[0];
		LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) m_private->buffer[0];
		if (buffer) {
			buffer->Release();
		}
		object->Release();
	}
	if (m_private->buffer[1]) {
		LPDIRECTSOUNDCAPTURE object = (LPDIRECTSOUNDCAPTURE) m_private->id[1];
		LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) m_private->buffer[1];
		if (buffer != nullptr) {
			buffer->Release();
		}
	}
	CloseHandle(m_private->condition);
	for (size_t iii=0; iii<2; ++iii) {
		m_userBuffer[iii].clear();
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	m_state = audio::orchestra::state::closed;
	return false;
}

enum audio::orchestra::error audio::orchestra::api::Ds::closeStream() {
	if (m_state == audio::orchestra::state::closed) {
		ATA_ERROR("no open stream to close!");
		return audio::orchestra::error_warning;
	}
	// Stop the callback thread.
	m_private->threadRunning = false;
	if (m_private->thread != nullptr) {
		m_private->thread->join();
		m_private->thread = nullptr;
	}
	if (m_private->buffer[0]) { // the object pointer can be nullptr and valid
		LPDIRECTSOUND object = (LPDIRECTSOUND) m_private->id[0];
		LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) m_private->buffer[0];
		if (buffer) {
			buffer->Stop();
			buffer->Release();
		}
		object->Release();
	}
	if (m_private->buffer[1]) {
		LPDIRECTSOUNDCAPTURE object = (LPDIRECTSOUNDCAPTURE) m_private->id[1];
		LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) m_private->buffer[1];
		if (buffer) {
			buffer->Stop();
			buffer->Release();
		}
		object->Release();
	}
	CloseHandle(m_private->condition);
	for (size_t iii=0; iii<2; ++iii) {
		m_userBuffer[iii].clear();
	}
	if (m_deviceBuffer) {
		free(m_deviceBuffer);
		m_deviceBuffer = 0;
	}
	m_mode = audio::orchestra::mode_unknow;
	m_state = audio::orchestra::state::closed;
}

enum audio::orchestra::error audio::orchestra::api::Ds::startStream() {
	// TODO : Check return ...
	audio::orchestra::Api::startStream();
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state::running) {
		ATA_ERROR("the stream is already running!");
		return audio::orchestra::error_warning;
	}
	// Increase scheduler frequency on lesser windows (a side-effect of
	// increasing timer accuracy).	On greater windows (Win2K or later),
	// this is already in effect.
	timeBeginPeriod(1); 
	m_buffersRolling = false;
	m_duplexPrerollBytes = 0;
	if (m_mode == audio::orchestra::mode_duplex) {
		// 0.5 seconds of silence in audio::orchestra::mode_duplex mode while the devices spin up and synchronize.
		m_duplexPrerollBytes = (int) (0.5 * m_sampleRate * audio::getFormatBytes(m_deviceFormat[1]) * m_nDeviceChannels[1]);
	}
	HRESULT result = 0;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) m_private->buffer[0];
		result = buffer->Play(0, 0, DSBPLAY_LOOPING);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": starting output buffer!");
			goto unlock;
		}
	}
	if (    m_mode == audio::orchestra::mode_input
	     || m_mode == audio::orchestra::mode_duplex) {
		LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) m_private->buffer[1];
		result = buffer->Start(DSCBSTART_LOOPING);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": starting input buffer!");
			goto unlock;
		}
	}
	m_private->drainCounter = 0;
	m_private->internalDrain = false;
	ResetEvent(m_private->condition);
	m_state = audio::orchestra::state::running;
unlock:
	if (FAILED(result)) {
		return audio::orchestra::error_systemError;
	}
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Ds::stopStream() {
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state::stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	HRESULT result = 0;
	LPVOID audioPtr;
	DWORD dataLen;
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		if (m_private->drainCounter == 0) {
			m_private->drainCounter = 2;
			WaitForSingleObject(m_private->condition, INFINITE);	// block until signaled
		}
		m_state = audio::orchestra::state::stopped;
		// Stop the buffer and clear memory
		LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) m_private->buffer[0];
		result = buffer->Stop();
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": stopping output buffer!");
			goto unlock;
		}
		// Lock the buffer and clear it so that if we start to play again,
		// we won't have old data playing.
		result = buffer->Lock(0, m_private->dsBufferSize[0], &audioPtr, &dataLen, nullptr, nullptr, 0);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": locking output buffer!");
			goto unlock;
		}
		// Zero the DS buffer
		ZeroMemory(audioPtr, dataLen);
		// Unlock the DS buffer
		result = buffer->Unlock(audioPtr, dataLen, nullptr, 0);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": unlocking output buffer!");
			goto unlock;
		}
		// If we start playing again, we must begin at beginning of buffer.
		m_private->bufferPointer[0] = 0;
	}
	if (    m_mode == audio::orchestra::mode_input
	     || m_mode == audio::orchestra::mode_duplex) {
		LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) m_private->buffer[1];
		audioPtr = nullptr;
		dataLen = 0;
		m_state = audio::orchestra::state::stopped;
		result = buffer->Stop();
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": stopping input buffer!");
			goto unlock;
		}
		// Lock the buffer and clear it so that if we start to play again,
		// we won't have old data playing.
		result = buffer->Lock(0, m_private->dsBufferSize[1], &audioPtr, &dataLen, nullptr, nullptr, 0);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": locking input buffer!");
			goto unlock;
		}
		// Zero the DS buffer
		ZeroMemory(audioPtr, dataLen);
		// Unlock the DS buffer
		result = buffer->Unlock(audioPtr, dataLen, nullptr, 0);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": unlocking input buffer!");
			goto unlock;
		}
		// If we start recording again, we must begin at beginning of buffer.
		m_private->bufferPointer[1] = 0;
	}
unlock:
	timeEndPeriod(1); // revert to normal scheduler frequency on lesser windows.
	if (FAILED(result)) {
		return audio::orchestra::error_systemError;
	}
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::api::Ds::abortStream() {
	if (verifyStream() != audio::orchestra::error_none) {
		return audio::orchestra::error_fail;
	}
	if (m_state == audio::orchestra::state::stopped) {
		ATA_ERROR("the stream is already stopped!");
		return audio::orchestra::error_warning;
	}
	m_private->drainCounter = 2;
	return stopStream();
}

void audio::orchestra::api::Ds::callbackEvent() {
	ethread::setName("DS IO-" + m_name);
	if (    m_state == audio::orchestra::state::stopped
	     || m_state == audio::orchestra::state::stopping) {
		Sleep(50); // sleep 50 milliseconds
		return;
	}
	if (m_state == audio::orchestra::state::closed) {
		ATA_ERROR("the stream is closed ... this shouldn't happen!");
		return;
	}
	// Check if we were draining the stream and signal is finished.
	if (m_private->drainCounter > m_nBuffers + 2) {
		m_state = audio::orchestra::state::stopping;
		if (m_private->internalDrain == false) {
			SetEvent(m_private->condition);
		} else {
			stopStream();
		}
		return;
	}
	// Invoke user callback to get fresh output data UNLESS we are
	// draining stream.
	if (m_private->drainCounter == 0) {
		audio::Time streamTime = getStreamTime();
		etk::Vector<audio::orchestra::status> status;
		if (    m_mode != audio::orchestra::mode_input
		     && m_private->xrun[0] == true) {
			status.pushBack(audio::orchestra::status::underflow);
			m_private->xrun[0] = false;
		}
		if (    m_mode != audio::orchestra::mode_output
		     && m_private->xrun[1] == true) {
			status.pushBack(audio::orchestra::status::overflow);
			m_private->xrun[1] = false;
		}
		int32_t cbReturnValue = m_callback(&m_userBuffer[1][0],
		                                   streamTime,
		                                   &m_userBuffer[0][0],
		                                   streamTime,
		                                   m_bufferSize,
		                                   status);
		if (cbReturnValue == 2) {
			m_state = audio::orchestra::state::stopping;
			m_private->drainCounter = 2;
			abortStream();
			return;
		} else if (cbReturnValue == 1) {
			m_private->drainCounter = 1;
			m_private->internalDrain = true;
		}
	}
	HRESULT result;
	DWORD currentWritePointer, safeWritePointer;
	DWORD currentReadPointer, safeReadPointer;
	UINT nextWritePointer;
	LPVOID buffer1 = nullptr;
	LPVOID buffer2 = nullptr;
	DWORD bufferSize1 = 0;
	DWORD bufferSize2 = 0;
	char *buffer;
	long bufferBytes;
	if (m_buffersRolling == false) {
		if (m_mode == audio::orchestra::mode_duplex) {
			//assert(m_private->dsBufferSize[0] == m_private->dsBufferSize[1]);
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
			LPDIRECTSOUNDBUFFER dsWriteBuffer = (LPDIRECTSOUNDBUFFER) m_private->buffer[0];
			LPDIRECTSOUNDCAPTUREBUFFER dsCaptureBuffer = (LPDIRECTSOUNDCAPTUREBUFFER) m_private->buffer[1];
			DWORD startSafeWritePointer, startSafeReadPointer;
			result = dsWriteBuffer->GetCurrentPosition(nullptr, &startSafeWritePointer);
			if (FAILED(result)) {
				ATA_ERROR(getErrorString(result) << ": getting current write position!");
				return;
			}
			result = dsCaptureBuffer->GetCurrentPosition(nullptr, &startSafeReadPointer);
			if (FAILED(result)) {
				ATA_ERROR(getErrorString(result) << ": getting current read position!");
				return;
			}
			while (true) {
				result = dsWriteBuffer->GetCurrentPosition(nullptr, &safeWritePointer);
				if (FAILED(result)) {
					ATA_ERROR(getErrorString(result) << ": getting current write position!");
					return;
				}
				result = dsCaptureBuffer->GetCurrentPosition(nullptr, &safeReadPointer);
				if (FAILED(result)) {
					ATA_ERROR(getErrorString(result) << ": getting current read position!");
					return;
				}
				if (    safeWritePointer != startSafeWritePointer
				     && safeReadPointer != startSafeReadPointer) {
					break;
				}
				Sleep(1);
			}
			//assert(m_private->dsBufferSize[0] == m_private->dsBufferSize[1]);
			m_private->bufferPointer[0] = safeWritePointer + m_private->dsPointerLeadTime[0];
			if (m_private->bufferPointer[0] >= m_private->dsBufferSize[0]) {
				m_private->bufferPointer[0] -= m_private->dsBufferSize[0];
			}
			m_private->bufferPointer[1] = safeReadPointer;
		} else if (m_mode == audio::orchestra::mode_output) {
			// Set the proper nextWritePosition after initial startup.
			LPDIRECTSOUNDBUFFER dsWriteBuffer = (LPDIRECTSOUNDBUFFER) m_private->buffer[0];
			result = dsWriteBuffer->GetCurrentPosition(&currentWritePointer, &safeWritePointer);
			if (FAILED(result)) {
				ATA_ERROR(getErrorString(result) << ": getting current write position!");
				return;
			}
			m_private->bufferPointer[0] = safeWritePointer + m_private->dsPointerLeadTime[0];
			if (m_private->bufferPointer[0] >= m_private->dsBufferSize[0]) {
				m_private->bufferPointer[0] -= m_private->dsBufferSize[0];
			}
		}
		m_buffersRolling = true;
	}
	if (    m_mode == audio::orchestra::mode_output
	     || m_mode == audio::orchestra::mode_duplex) {
		LPDIRECTSOUNDBUFFER dsBuffer = (LPDIRECTSOUNDBUFFER) m_private->buffer[0];
		if (m_private->drainCounter > 1) { // write zeros to the output stream
			bufferBytes = m_bufferSize * m_nUserChannels[0];
			bufferBytes *= audio::getFormatBytes(m_userFormat);
			memset(&m_userBuffer[0][0], 0, bufferBytes);
		}
		// Setup parameters and do buffer conversion if necessary.
		if (m_doConvertBuffer[0]) {
			buffer = m_deviceBuffer;
			convertBuffer(buffer, &m_userBuffer[0][0], m_convertInfo[0]);
			bufferBytes = m_bufferSize * m_nDeviceChannels[0];
			bufferBytes *= audio::getFormatBytes(m_deviceFormat[0]);
		} else {
			buffer = &m_userBuffer[0][0];
			bufferBytes = m_bufferSize * m_nUserChannels[0];
			bufferBytes *= audio::getFormatBytes(m_userFormat);
		}
		// No byte swapping necessary in DirectSound implementation.
		// Ahhh ... windoze.	16-bit data is signed but 8-bit data is
		// unsigned.	So, we need to convert our signed 8-bit data here to
		// unsigned.
		if (m_deviceFormat[0] == audio::format_int8) {
			for (size_t iii=0; iii<bufferBytes; ++iii) {
				buffer[iii] = buffer[iii] + 128;
			}
		}
		DWORD dsBufferSize = m_private->dsBufferSize[0];
		nextWritePointer = m_private->bufferPointer[0];
		DWORD endWrite, leadPointer;
		while (true) {
			// Find out where the read and "safe write" pointers are.
			result = dsBuffer->GetCurrentPosition(&currentWritePointer, &safeWritePointer);
			if (FAILED(result)) {
				ATA_ERROR(getErrorString(result) << ": getting current write position!");
				return;
			}
			// We will copy our output buffer into the region between
			// safeWritePointer and leadPointer.	If leadPointer is not
			// beyond the next endWrite position, wait until it is.
			leadPointer = safeWritePointer + m_private->dsPointerLeadTime[0];
			//std::cout << "safeWritePointer = " << safeWritePointer << ", leadPointer = " << leadPointer << ", nextWritePointer = " << nextWritePointer << std::endl;
			if (leadPointer > dsBufferSize) {
				leadPointer -= dsBufferSize;
			}
			if (leadPointer < nextWritePointer) {
				leadPointer += dsBufferSize; // unwrap offset
			}
			endWrite = nextWritePointer + bufferBytes;
			// Check whether the entire write region is behind the play pointer.
			if (leadPointer >= endWrite) {
				break;
			}
			// If we are here, then we must wait until the leadPointer advances
			// beyond the end of our next write region. We use the
			// Sleep() function to suspend operation until that happens.
			double millis = (endWrite - leadPointer) * 1000.0;
			millis /= (audio::getFormatBytes(m_deviceFormat[0]) * m_nDeviceChannels[0] * m_sampleRate);
			if (millis < 1.0) {
				millis = 1.0;
			}
			Sleep((DWORD) millis);
		}
		if (    dsPointerBetween(nextWritePointer, safeWritePointer, currentWritePointer, dsBufferSize)
		     || dsPointerBetween(endWrite, safeWritePointer, currentWritePointer, dsBufferSize)) { 
			// We've strayed into the forbidden zone ... resync the read pointer.
			m_private->xrun[0] = true;
			nextWritePointer = safeWritePointer + m_private->dsPointerLeadTime[0] - bufferBytes;
			if (nextWritePointer >= dsBufferSize) {
				nextWritePointer -= dsBufferSize;
			}
			m_private->bufferPointer[0] = nextWritePointer;
			endWrite = nextWritePointer + bufferBytes;
		}
		// Lock free space in the buffer
		result = dsBuffer->Lock(nextWritePointer,
		                        bufferBytes,
		                        &buffer1,
		                        &bufferSize1,
		                        &buffer2,
		                        &bufferSize2,
		                        0);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": locking buffer during playback!");
			return;
		}
		// Copy our buffer into the DS buffer
		CopyMemory(buffer1, buffer, bufferSize1);
		if (buffer2 != nullptr) {
			CopyMemory(buffer2, buffer+bufferSize1, bufferSize2);
		}
		// Update our buffer offset and unlock sound buffer
		dsBuffer->Unlock(buffer1, bufferSize1, buffer2, bufferSize2);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": unlocking buffer during playback!");
			return;
		}
		nextWritePointer = (nextWritePointer + bufferSize1 + bufferSize2) % dsBufferSize;
		m_private->bufferPointer[0] = nextWritePointer;
		if (m_private->drainCounter) {
			m_private->drainCounter++;
			goto unlock;
		}
	}
	if (    m_mode == audio::orchestra::mode_input
	     || m_mode == audio::orchestra::mode_duplex) {
		// Setup parameters.
		if (m_doConvertBuffer[1]) {
			buffer = m_deviceBuffer;
			bufferBytes = m_bufferSize * m_nDeviceChannels[1];
			bufferBytes *= audio::getFormatBytes(m_deviceFormat[1]);
		} else {
			buffer = &m_userBuffer[1][0];
			bufferBytes = m_bufferSize * m_nUserChannels[1];
			bufferBytes *= audio::getFormatBytes(m_userFormat);
		}
		LPDIRECTSOUNDCAPTUREBUFFER dsBuffer = (LPDIRECTSOUNDCAPTUREBUFFER) m_private->buffer[1];
		long nextReadPointer = m_private->bufferPointer[1];
		DWORD dsBufferSize = m_private->dsBufferSize[1];
		// Find out where the write and "safe read" pointers are.
		result = dsBuffer->GetCurrentPosition(&currentReadPointer, &safeReadPointer);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": getting current read position!");
			return;
		}
		if (safeReadPointer < (DWORD)nextReadPointer) {
			safeReadPointer += dsBufferSize; // unwrap offset
		}
		DWORD endRead = nextReadPointer + bufferBytes;
		// Handling depends on whether we are audio::orchestra::mode_input or audio::orchestra::mode_duplex. 
		// If we're in audio::orchestra::mode_input mode then waiting is a good thing. If we're in audio::orchestra::mode_duplex mode,
		// then a wait here will drag the write pointers into the forbidden zone.
		// 
		// In audio::orchestra::mode_duplex mode, rather than wait, we will back off the read pointer until 
		// it's in a safe position. This causes dropouts, but it seems to be the only 
		// practical way to sync up the read and write pointers reliably, given the 
		// the very complex relationship between phase and increment of the read and write 
		// pointers.
		//
		// In order to minimize audible dropouts in audio::orchestra::mode_duplex mode, we will
		// provide a pre-roll period of 0.5 seconds in which we return
		// zeros from the read buffer while the pointers sync up.
		if (m_mode == audio::orchestra::mode_duplex) {
			if (safeReadPointer < endRead) {
				if (m_duplexPrerollBytes <= 0) {
					// Pre-roll time over. Be more agressive.
					int32_t adjustment = endRead-safeReadPointer;
					m_private->xrun[1] = true;
					// Two cases:
					//	 - large adjustments: we've probably run out of CPU cycles, so just resync exactly,
					//		 and perform fine adjustments later.
					//	 - small adjustments: back off by twice as much.
					if (adjustment >= 2*bufferBytes) {
						nextReadPointer = safeReadPointer-2*bufferBytes;
					} else {
						nextReadPointer = safeReadPointer-bufferBytes-adjustment;
					}
					if (nextReadPointer < 0) {
						nextReadPointer += dsBufferSize;
					}
				} else {
					// In pre=roll time. Just do it.
					nextReadPointer = safeReadPointer - bufferBytes;
					while (nextReadPointer < 0) {
						nextReadPointer += dsBufferSize;
					}
				}
				endRead = nextReadPointer + bufferBytes;
			}
		} else { // _mode == audio::orchestra::mode_input
			while (    safeReadPointer < endRead
			        && m_private->threadRunning) {
				// See comments for playback.
				double millis = (endRead - safeReadPointer) * 1000.0;
				millis /= (audio::getFormatBytes(m_deviceFormat[1]) * m_nDeviceChannels[1] * m_sampleRate);
				if (millis < 1.0) {
					millis = 1.0;
				}
				Sleep((DWORD) millis);
				// Wake up and find out where we are now.
				result = dsBuffer->GetCurrentPosition(&currentReadPointer, &safeReadPointer);
				if (FAILED(result)) {
					ATA_ERROR(getErrorString(result) << ": getting current read position!");
					return;
				}
				if (safeReadPointer < (DWORD)nextReadPointer) {
					// unwrap offset
					safeReadPointer += dsBufferSize;
				}
			}
		}
		// Lock free space in the buffer
		result = dsBuffer->Lock(nextReadPointer,
		                        bufferBytes,
		                        &buffer1,
		                        &bufferSize1,
		                        &buffer2,
		                        &bufferSize2,
		                        0);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": locking capture buffer!");
			return;
		}
		if (m_duplexPrerollBytes <= 0) {
			// Copy our buffer into the DS buffer
			CopyMemory(buffer, buffer1, bufferSize1);
			if (buffer2 != nullptr) {
				CopyMemory(buffer+bufferSize1, buffer2, bufferSize2);
			}
		} else {
			memset(buffer, 0, bufferSize1);
			if (buffer2 != nullptr) {
				memset(buffer + bufferSize1, 0, bufferSize2);
			}
			m_duplexPrerollBytes -= bufferSize1 + bufferSize2;
		}
		// Update our buffer offset and unlock sound buffer
		nextReadPointer = (nextReadPointer + bufferSize1 + bufferSize2) % dsBufferSize;
		dsBuffer->Unlock(buffer1, bufferSize1, buffer2, bufferSize2);
		if (FAILED(result)) {
			ATA_ERROR(getErrorString(result) << ": unlocking capture buffer!");
			return;
		}
		m_private->bufferPointer[1] = nextReadPointer;
		// No byte swapping necessary in DirectSound implementation.
		// If necessary, convert 8-bit data from unsigned to signed.
		if (m_deviceFormat[1] == audio::format_int8) {
			for (size_t jjj=0; jjj<bufferBytes; ++jjj) {
				buffer[jjj] = (signed char) (buffer[jjj] - 128);
			}
		}
		// Do buffer conversion if necessary.
		if (m_doConvertBuffer[1]) {
			convertBuffer(&m_userBuffer[1][0], m_deviceBuffer, m_convertInfo[1]);
		}
	}
unlock:
	audio::orchestra::Api::tickStreamTime();
}

void audio::orchestra::api::Ds::dsCallbackEvent(void *_userData) {
	audio::orchestra::api::Ds* myClass = reinterpret_cast<audio::orchestra::api::Ds*>(_userData);
	while (myClass->m_private->threadRunning == true) {
		myClass->callbackEvent();
	}
}

static const char* getErrorString(int32_t code) {
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

#endif
