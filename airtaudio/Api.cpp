/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

//#include <etk/types.h>
#include <airtaudio/Interface.h>
#include <airtaudio/debug.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <climits>

#undef __class__
#define __class__ "api"


// Static variable definitions.
const std::vector<uint32_t>& airtaudio::genericSampleRate() {
	static std::vector<uint32_t> list;
	if (list.size() == 0) {
		list.push_back(4000);
		list.push_back(5512);
		list.push_back(8000);
		list.push_back(9600);
		list.push_back(11025);
		list.push_back(16000);
		list.push_back(22050);
		list.push_back(32000);
		list.push_back(44100);
		list.push_back(48000);
		list.push_back(64000);
		list.push_back(88200);
		list.push_back(96000);
		list.push_back(128000);
		list.push_back(176400);
		list.push_back(192000);
	}
	return list;
};


airtaudio::Api::Api() :
  m_callback(nullptr),
  m_deviceBuffer(nullptr) {
	m_device[0] = 11111;
	m_device[1] = 11111;
	m_state = airtaudio::state_closed;
	m_mode = airtaudio::mode_unknow;
}

airtaudio::Api::~Api() {
	
}

enum airtaudio::error airtaudio::Api::startStream() {
	ATA_VERBOSE("Start Stream");
	m_startTime = std::chrono::system_clock::now();
	m_duration = std::chrono::duration<int64_t, std::micro>(0);
	return airtaudio::error_none;
}

enum airtaudio::error airtaudio::Api::openStream(airtaudio::StreamParameters *oParams,
                                                 airtaudio::StreamParameters *iParams,
                                                 enum audio::format format,
                                                 uint32_t sampleRate,
                                                 uint32_t *bufferFrames,
                                                 airtaudio::AirTAudioCallback callback,
                                                 airtaudio::StreamOptions *options) {
	if (m_state != airtaudio::state_closed) {
		ATA_ERROR("a stream is already open!");
		return airtaudio::error_invalidUse;
	}
	if (oParams && oParams->nChannels < 1) {
		ATA_ERROR("a non-nullptr output StreamParameters structure cannot have an nChannels value less than one.");
		return airtaudio::error_invalidUse;
	}
	if (iParams && iParams->nChannels < 1) {
		ATA_ERROR("a non-nullptr input StreamParameters structure cannot have an nChannels value less than one.");
		return airtaudio::error_invalidUse;
	}
	if (oParams == nullptr && iParams == nullptr) {
		ATA_ERROR("input and output StreamParameters structures are both nullptr!");
		return airtaudio::error_invalidUse;
	}
	if (audio::getFormatBytes(format) == 0) {
		ATA_ERROR("'format' parameter value is undefined.");
		return airtaudio::error_invalidUse;
	}
	uint32_t nDevices = getDeviceCount();
	uint32_t oChannels = 0;
	if (oParams) {
		oChannels = oParams->nChannels;
		if (oParams->deviceId >= nDevices) {
			ATA_ERROR("output device parameter value is invalid.");
			return airtaudio::error_invalidUse;
		}
	}
	uint32_t iChannels = 0;
	if (iParams) {
		iChannels = iParams->nChannels;
		if (iParams->deviceId >= nDevices) {
			ATA_ERROR("input device parameter value is invalid.");
			return airtaudio::error_invalidUse;
		}
	}
	clearStreamInfo();
	bool result;
	if (oChannels > 0) {
		result = probeDeviceOpen(oParams->deviceId,
		                         airtaudio::mode_output,
		                         oChannels,
		                         oParams->firstChannel,
		                         sampleRate,
		                         format,
		                         bufferFrames,
		                         options);
		if (result == false) {
			ATA_ERROR("system ERROR");
			return airtaudio::error_systemError;
		}
	}
	if (iChannels > 0) {
		result = probeDeviceOpen(iParams->deviceId,
		                         airtaudio::mode_input,
		                         iChannels,
		                         iParams->firstChannel,
		                         sampleRate,
		                         format,
		                         bufferFrames,
		                         options);
		if (result == false) {
			if (oChannels > 0) {
				closeStream();
			}
			ATA_ERROR("system error");
			return airtaudio::error_systemError;
		}
	}
	m_callback = callback;
	if (options != nullptr) {
		options->numberOfBuffers = m_nBuffers;
	}
	m_state = airtaudio::state_stopped;
	return airtaudio::error_none;
}

uint32_t airtaudio::Api::getDefaultInputDevice() {
	// Should be implemented in subclasses if possible.
	return 0;
}

uint32_t airtaudio::Api::getDefaultOutputDevice() {
	// Should be implemented in subclasses if possible.
	return 0;
}

enum airtaudio::error airtaudio::Api::closeStream() {
	ATA_VERBOSE("Close Stream");
	// MUST be implemented in subclasses!
	return airtaudio::error_none;
}

bool airtaudio::Api::probeDeviceOpen(uint32_t /*device*/,
                                     airtaudio::mode /*mode*/,
                                     uint32_t /*channels*/,
                                     uint32_t /*firstChannel*/,
                                     uint32_t /*sampleRate*/,
                                     audio::format /*format*/,
                                     uint32_t * /*bufferSize*/,
                                     airtaudio::StreamOptions * /*options*/) {
	// MUST be implemented in subclasses!
	return false;
}

void airtaudio::Api::tickStreamTime() {
	//ATA_WARNING("tick : size=" << m_bufferSize << " rate=" << m_sampleRate << " time=" << std::chrono::nanoseconds((int64_t(m_bufferSize) * int64_t(1000000000)) / int64_t(m_sampleRate)).count());
	//ATA_WARNING("  one element=" << std::chrono::nanoseconds((int64_t(1000000000)) / int64_t(m_sampleRate)).count());
	m_duration += std::chrono::nanoseconds((int64_t(m_bufferSize) * int64_t(1000000000)) / int64_t(m_sampleRate));
}

long airtaudio::Api::getStreamLatency() {
	if (verifyStream() != airtaudio::error_none) {
		return 0;
	}
	long totalLatency = 0;
	if (    m_mode == airtaudio::mode_output
	     || m_mode == airtaudio::mode_duplex) {
		totalLatency = m_latency[0];
	}
	if (    m_mode == airtaudio::mode_input
	     || m_mode == airtaudio::mode_duplex) {
		totalLatency += m_latency[1];
	}
	return totalLatency;
}

std::chrono::system_clock::time_point airtaudio::Api::getStreamTime() {
	if (verifyStream() != airtaudio::error_none) {
		return std::chrono::system_clock::time_point();
	}
	return m_startTime + m_duration;
}

uint32_t airtaudio::Api::getStreamSampleRate() {
	if (verifyStream() != airtaudio::error_none) {
		return 0;
	}
	return m_sampleRate;
}

enum airtaudio::error airtaudio::Api::verifyStream() {
	if (m_state == airtaudio::state_closed) {
		ATA_ERROR("a stream is not open!");
		return airtaudio::error_invalidUse;
	}
	return airtaudio::error_none;
}

void airtaudio::Api::clearStreamInfo() {
	m_mode = airtaudio::mode_unknow;
	m_state = airtaudio::state_closed;
	m_sampleRate = 0;
	m_bufferSize = 0;
	m_nBuffers = 0;
	m_userFormat = audio::format_unknow;
	m_startTime = std::chrono::system_clock::time_point();
	m_duration = std::chrono::nanoseconds(0);
	m_deviceBuffer = nullptr;
	m_callback = nullptr;
	for (int32_t iii=0; iii<2; ++iii) {
		m_device[iii] = 11111;
		m_doConvertBuffer[iii] = false;
		m_deviceInterleaved[iii] = true;
		m_doByteSwap[iii] = false;
		m_nUserChannels[iii] = 0;
		m_nDeviceChannels[iii] = 0;
		m_channelOffset[iii] = 0;
		m_deviceFormat[iii] = audio::format_unknow;
		m_latency[iii] = 0;
		m_userBuffer[iii].clear();
		m_convertInfo[iii].channels = 0;
		m_convertInfo[iii].inJump = 0;
		m_convertInfo[iii].outJump = 0;
		m_convertInfo[iii].inFormat = audio::format_unknow;
		m_convertInfo[iii].outFormat = audio::format_unknow;
		m_convertInfo[iii].inOffset.clear();
		m_convertInfo[iii].outOffset.clear();
	}
}

void airtaudio::Api::setConvertInfo(airtaudio::mode _mode, uint32_t _firstChannel) {
	int32_t idTable = airtaudio::modeToIdTable(_mode);
	if (_mode == airtaudio::mode_input) { // convert device to user buffer
		m_convertInfo[idTable].inJump = m_nDeviceChannels[1];
		m_convertInfo[idTable].outJump = m_nUserChannels[1];
		m_convertInfo[idTable].inFormat = m_deviceFormat[1];
		m_convertInfo[idTable].outFormat = m_userFormat;
	} else { // convert user to device buffer
		m_convertInfo[idTable].inJump = m_nUserChannels[0];
		m_convertInfo[idTable].outJump = m_nDeviceChannels[0];
		m_convertInfo[idTable].inFormat = m_userFormat;
		m_convertInfo[idTable].outFormat = m_deviceFormat[0];
	}
	if (m_convertInfo[idTable].inJump < m_convertInfo[idTable].outJump) {
		m_convertInfo[idTable].channels = m_convertInfo[idTable].inJump;
	} else {
		m_convertInfo[idTable].channels = m_convertInfo[idTable].outJump;
	}
	// Set up the interleave/deinterleave offsets.
	if (m_deviceInterleaved[idTable] == false) {
		if (_mode == airtaudio::mode_input) {
			for (int32_t kkk=0; kkk<m_convertInfo[idTable].channels; ++kkk) {
				m_convertInfo[idTable].inOffset.push_back(kkk * m_bufferSize);
				m_convertInfo[idTable].outOffset.push_back(kkk);
				m_convertInfo[idTable].inJump = 1;
			}
		} else {
			for (int32_t kkk=0; kkk<m_convertInfo[idTable].channels; ++kkk) {
				m_convertInfo[idTable].inOffset.push_back(kkk);
				m_convertInfo[idTable].outOffset.push_back(kkk * m_bufferSize);
				m_convertInfo[idTable].outJump = 1;
			}
		}
	} else { // no (de)interleaving
		for (int32_t kkk=0; kkk<m_convertInfo[idTable].channels; ++kkk) {
			m_convertInfo[idTable].inOffset.push_back(kkk);
			m_convertInfo[idTable].outOffset.push_back(kkk);
		}
	}

	// Add channel offset.
	if (_firstChannel > 0) {
		if (m_deviceInterleaved[idTable]) {
			if (_mode == airtaudio::mode_output) {
				for (int32_t kkk=0; kkk<m_convertInfo[idTable].channels; ++kkk) {
					m_convertInfo[idTable].outOffset[kkk] += _firstChannel;
				}
			} else {
				for (int32_t kkk=0; kkk<m_convertInfo[idTable].channels; ++kkk) {
					m_convertInfo[idTable].inOffset[kkk] += _firstChannel;
				}
			}
		} else {
			if (_mode == airtaudio::mode_output) {
				for (int32_t kkk=0; kkk<m_convertInfo[idTable].channels; ++kkk) {
					m_convertInfo[idTable].outOffset[kkk] += (_firstChannel * m_bufferSize);
				}
			} else {
				for (int32_t kkk=0; kkk<m_convertInfo[idTable].channels; ++kkk) {
					m_convertInfo[idTable].inOffset[kkk] += (_firstChannel * m_bufferSize);
				}
			}
		}
	}
}

void airtaudio::Api::convertBuffer(char *_outBuffer, char *_inBuffer, airtaudio::ConvertInfo &_info) {
	// This function does format conversion, input/output channel compensation, and
	// data interleaving/deinterleaving.	24-bit integers are assumed to occupy
	// the lower three bytes of a 32-bit integer.

	// Clear our device buffer when in/out duplex device channels are different
	if (    _outBuffer == m_deviceBuffer
	     && m_mode == airtaudio::mode_duplex
	     && m_nDeviceChannels[0] < m_nDeviceChannels[1]) {
		memset(_outBuffer, 0, m_bufferSize * _info.outJump * audio::getFormatBytes(_info.outFormat));
	}
	switch (audio::getFormatBytes(_info.outFormat)) {
		case 1:
			{
				uint8_t *out = reinterpret_cast<uint8_t*>(_outBuffer);
				uint8_t *in = reinterpret_cast<uint8_t*>(_inBuffer);
				for (size_t iii=0; iii<m_bufferSize; ++iii) {
					for (size_t jjj=0; jjj<_info.channels; jjj++) {
						out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
					}
					in += _info.inJump;
					out += _info.outJump;
				}
			}
			break;
		case 2:
			{
				uint16_t *out = reinterpret_cast<uint16_t*>(_outBuffer);
				uint16_t *in = reinterpret_cast<uint16_t*>(_inBuffer);
				for (size_t iii=0; iii<m_bufferSize; ++iii) {
					for (size_t jjj=0; jjj<_info.channels; jjj++) {
						out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
					}
					in += _info.inJump;
					out += _info.outJump;
				}
			}
			break;
		case 4:
			{
				uint32_t *out = reinterpret_cast<uint32_t*>(_outBuffer);
				uint32_t *in = reinterpret_cast<uint32_t*>(_inBuffer);
				for (size_t iii=0; iii<m_bufferSize; ++iii) {
					for (size_t jjj=0; jjj<_info.channels; jjj++) {
						out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
					}
					in += _info.inJump;
					out += _info.outJump;
				}
			}
			break;
		case 8:
			{
				uint64_t *out = reinterpret_cast<uint64_t*>(_outBuffer);
				uint64_t *in = reinterpret_cast<uint64_t*>(_inBuffer);
				for (size_t iii=0; iii<m_bufferSize; ++iii) {
					for (size_t jjj=0; jjj<_info.channels; jjj++) {
						out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
					}
					in += _info.inJump;
					out += _info.outJump;
				}
			}
			break;
	}
}

void airtaudio::Api::byteSwapBuffer(char *_buffer, uint32_t _samples, audio::format _format) {
	char val;
	char *ptr;
	ptr = _buffer;
	if (_format == audio::format_int16) {
		for (uint32_t iii=0; iii<_samples; ++iii) {
			// Swap 1st and 2nd bytes.
			val = *(ptr);
			*(ptr) = *(ptr+1);
			*(ptr+1) = val;

			// Increment 2 bytes.
			ptr += 2;
		}
	} else if (    _format == audio::format_int32
	            || _format == audio::format_float) {
		for (uint32_t iii=0; iii<_samples; ++iii) {
			// Swap 1st and 4th bytes.
			val = *(ptr);
			*(ptr) = *(ptr+3);
			*(ptr+3) = val;

			// Swap 2nd and 3rd bytes.
			ptr += 1;
			val = *(ptr);
			*(ptr) = *(ptr+1);
			*(ptr+1) = val;

			// Increment 3 more bytes.
			ptr += 3;
		}
	} else if (_format == audio::format_int24) {
		for (uint32_t iii=0; iii<_samples; ++iii) {
			// Swap 1st and 3rd bytes.
			val = *(ptr);
			*(ptr) = *(ptr+2);
			*(ptr+2) = val;

			// Increment 2 more bytes.
			ptr += 2;
		}
	} else if (_format == audio::format_double) {
		for (uint32_t iii=0; iii<_samples; ++iii) {
			// Swap 1st and 8th bytes
			val = *(ptr);
			*(ptr) = *(ptr+7);
			*(ptr+7) = val;

			// Swap 2nd and 7th bytes
			ptr += 1;
			val = *(ptr);
			*(ptr) = *(ptr+5);
			*(ptr+5) = val;

			// Swap 3rd and 6th bytes
			ptr += 1;
			val = *(ptr);
			*(ptr) = *(ptr+3);
			*(ptr+3) = val;

			// Swap 4th and 5th bytes
			ptr += 1;
			val = *(ptr);
			*(ptr) = *(ptr+1);
			*(ptr+1) = val;

			// Increment 5 more bytes.
			ptr += 5;
		}
	}
}


