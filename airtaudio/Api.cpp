/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
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

static const char* listType[] {
	"undefined",
	"alsa",
	"pulse",
	"oss",
	"jack",
	"coreOSX",
	"corIOS",
	"asio",
	"ds",
	"java",
	"dummy",
	"user1",
	"user2",
	"user3",
	"user4"
};
static int32_t listTypeSize = sizeof(listType)/sizeof(char*);


std::ostream& airtaudio::operator <<(std::ostream& _os, const enum airtaudio::type& _obj) {
	_os << listType[_obj];
	return _os;
}

std::ostream& airtaudio::operator <<(std::ostream& _os, const std::vector<enum airtaudio::type>& _obj) {
	_os << std::string("{");
	for (size_t iii=0; iii<_obj.size(); ++iii) {
		if (iii!=0) {
			_os << std::string(";");
		}
		_os << _obj[iii];
	}
	_os << std::string("}");
	return _os;
}

std::string airtaudio::getTypeString(enum audio::format _value) {
	return listType[_value];
}

enum airtaudio::type airtaudio::getTypeFromString(const std::string& _value) {
	for (int32_t iii=0; iii<listTypeSize; ++iii) {
		if (_value == listType[iii]) {
			return static_cast<enum airtaudio::type>(iii);
		}
	}
	return airtaudio::type_undefined;
}

int32_t airtaudio::modeToIdTable(enum mode _mode) {
	switch (_mode) {
		case mode_unknow:
		case mode_duplex:
		case mode_output:
			return 0;
		case mode_input:
			return 1;
	}
	return 0;
}

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


airtaudio::Api::Api() {
	m_stream.state = airtaudio::state_closed;
	m_stream.mode = airtaudio::mode_unknow;
	m_stream.apiHandle = 0;
	m_stream.userBuffer[0] = 0;
	m_stream.userBuffer[1] = 0;
}

airtaudio::Api::~Api() {
	
}

enum airtaudio::error airtaudio::Api::openStream(airtaudio::StreamParameters *oParams,
                                                     airtaudio::StreamParameters *iParams,
                                                     enum audio::format format,
                                                     uint32_t sampleRate,
                                                     uint32_t *bufferFrames,
                                                     airtaudio::AirTAudioCallback callback,
                                                     airtaudio::StreamOptions *options) {
	if (m_stream.state != airtaudio::state_closed) {
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
	m_stream.callbackInfo.callback = callback;
	if (options != nullptr) {
		options->numberOfBuffers = m_stream.nBuffers;
	}
	m_stream.state = airtaudio::state_stopped;
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
	// Subclasses that do not provide their own implementation of
	// getStreamTime should call this function once per buffer I/O to
	// provide basic stream time support.
	m_stream.streamTime += (m_stream.bufferSize * 1.0 / m_stream.sampleRate);
#if defined(HAVE_GETTIMEOFDAY)
	gettimeofday(&m_stream.lastTickTimestamp, nullptr);
#endif
}

long airtaudio::Api::getStreamLatency() {
	if (verifyStream() != airtaudio::error_none) {
		return 0;
	}
	long totalLatency = 0;
	if (    m_stream.mode == airtaudio::mode_output
	     || m_stream.mode == airtaudio::mode_duplex) {
		totalLatency = m_stream.latency[0];
	}
	if (    m_stream.mode == airtaudio::mode_input
	     || m_stream.mode == airtaudio::mode_duplex) {
		totalLatency += m_stream.latency[1];
	}
	return totalLatency;
}

double airtaudio::Api::getStreamTime() {
	if (verifyStream() != airtaudio::error_none) {
		return 0.0f;
	}
#if defined(HAVE_GETTIMEOFDAY)
	// Return a very accurate estimate of the stream time by
	// adding in the elapsed time since the last tick.
	struct timeval then;
	struct timeval now;
	if (m_stream.state != airtaudio::state_running || m_stream.streamTime == 0.0) {
		return m_stream.streamTime;
	}
	gettimeofday(&now, nullptr);
	then = m_stream.lastTickTimestamp;
	return   m_stream.streamTime
	       + ((now.tv_sec + 0.000001 * now.tv_usec)
	       - (then.tv_sec + 0.000001 * then.tv_usec));
#else
	return m_stream.streamTime;
#endif
}

uint32_t airtaudio::Api::getStreamSampleRate() {
	if (verifyStream() != airtaudio::error_none) {
		return 0;
	}
	return m_stream.sampleRate;
}

enum airtaudio::error airtaudio::Api::verifyStream() {
	if (m_stream.state == airtaudio::state_closed) {
		ATA_ERROR("a stream is not open!");
		return airtaudio::error_invalidUse;
	}
	return airtaudio::error_none;
}

void airtaudio::Api::clearStreamInfo() {
	m_stream.mode = airtaudio::mode_unknow;
	m_stream.state = airtaudio::state_closed;
	m_stream.sampleRate = 0;
	m_stream.bufferSize = 0;
	m_stream.nBuffers = 0;
	m_stream.userFormat = audio::format_unknow;
	m_stream.streamTime = 0.0;
	m_stream.apiHandle = 0;
	m_stream.deviceBuffer = 0;
	m_stream.callbackInfo.callback = 0;
	m_stream.callbackInfo.isRunning = false;
	for (int32_t iii=0; iii<2; ++iii) {
		m_stream.device[iii] = 11111;
		m_stream.doConvertBuffer[iii] = false;
		m_stream.deviceInterleaved[iii] = true;
		m_stream.doByteSwap[iii] = false;
		m_stream.nUserChannels[iii] = 0;
		m_stream.nDeviceChannels[iii] = 0;
		m_stream.channelOffset[iii] = 0;
		m_stream.deviceFormat[iii] = audio::format_unknow;
		m_stream.latency[iii] = 0;
		m_stream.userBuffer[iii] = 0;
		m_stream.convertInfo[iii].channels = 0;
		m_stream.convertInfo[iii].inJump = 0;
		m_stream.convertInfo[iii].outJump = 0;
		m_stream.convertInfo[iii].inFormat = audio::format_unknow;
		m_stream.convertInfo[iii].outFormat = audio::format_unknow;
		m_stream.convertInfo[iii].inOffset.clear();
		m_stream.convertInfo[iii].outOffset.clear();
	}
}

void airtaudio::Api::setConvertInfo(airtaudio::mode _mode, uint32_t _firstChannel) {
	int32_t idTable = airtaudio::modeToIdTable(_mode);
	if (_mode == airtaudio::mode_input) { // convert device to user buffer
		m_stream.convertInfo[idTable].inJump = m_stream.nDeviceChannels[1];
		m_stream.convertInfo[idTable].outJump = m_stream.nUserChannels[1];
		m_stream.convertInfo[idTable].inFormat = m_stream.deviceFormat[1];
		m_stream.convertInfo[idTable].outFormat = m_stream.userFormat;
	} else { // convert user to device buffer
		m_stream.convertInfo[idTable].inJump = m_stream.nUserChannels[0];
		m_stream.convertInfo[idTable].outJump = m_stream.nDeviceChannels[0];
		m_stream.convertInfo[idTable].inFormat = m_stream.userFormat;
		m_stream.convertInfo[idTable].outFormat = m_stream.deviceFormat[0];
	}
	if (m_stream.convertInfo[idTable].inJump < m_stream.convertInfo[idTable].outJump) {
		m_stream.convertInfo[idTable].channels = m_stream.convertInfo[idTable].inJump;
	} else {
		m_stream.convertInfo[idTable].channels = m_stream.convertInfo[idTable].outJump;
	}
	// Set up the interleave/deinterleave offsets.
	if (m_stream.deviceInterleaved[idTable] == false) {
		if (_mode == airtaudio::mode_input) {
			for (int32_t kkk=0; kkk<m_stream.convertInfo[idTable].channels; ++kkk) {
				m_stream.convertInfo[idTable].inOffset.push_back(kkk * m_stream.bufferSize);
				m_stream.convertInfo[idTable].outOffset.push_back(kkk);
				m_stream.convertInfo[idTable].inJump = 1;
			}
		} else {
			for (int32_t kkk=0; kkk<m_stream.convertInfo[idTable].channels; ++kkk) {
				m_stream.convertInfo[idTable].inOffset.push_back(kkk);
				m_stream.convertInfo[idTable].outOffset.push_back(kkk * m_stream.bufferSize);
				m_stream.convertInfo[idTable].outJump = 1;
			}
		}
	} else { // no (de)interleaving
		for (int32_t kkk=0; kkk<m_stream.convertInfo[idTable].channels; ++kkk) {
			m_stream.convertInfo[idTable].inOffset.push_back(kkk);
			m_stream.convertInfo[idTable].outOffset.push_back(kkk);
		}
	}

	// Add channel offset.
	if (_firstChannel > 0) {
		if (m_stream.deviceInterleaved[idTable]) {
			if (_mode == airtaudio::mode_output) {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[idTable].channels; ++kkk) {
					m_stream.convertInfo[idTable].outOffset[kkk] += _firstChannel;
				}
			} else {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[idTable].channels; ++kkk) {
					m_stream.convertInfo[idTable].inOffset[kkk] += _firstChannel;
				}
			}
		} else {
			if (_mode == airtaudio::mode_output) {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[idTable].channels; ++kkk) {
					m_stream.convertInfo[idTable].outOffset[kkk] += (_firstChannel * m_stream.bufferSize);
				}
			} else {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[idTable].channels; ++kkk) {
					m_stream.convertInfo[idTable].inOffset[kkk] += (_firstChannel * m_stream.bufferSize);
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
	if (    _outBuffer == m_stream.deviceBuffer
	     && m_stream.mode == airtaudio::mode_duplex
	     && m_stream.nDeviceChannels[0] < m_stream.nDeviceChannels[1]) {
		memset(_outBuffer, 0, m_stream.bufferSize * _info.outJump * audio::getFormatBytes(_info.outFormat));
	}
	int32_t jjj;
	if (_info.outFormat != _info.outFormat) {
		ATA_CRITICAL("not manage anymore the format changing ...");
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


