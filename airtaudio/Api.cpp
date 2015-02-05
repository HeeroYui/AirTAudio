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

std::ostream& operator <<(std::ostream& _os, const airtaudio::type& _obj){
	_os << listType[_obj];
	return _os;
}

// Static variable definitions.
static const uint32_t MAX_SAMPLE_RATES = 14;
static const uint32_t SAMPLE_RATES[] = {
	4000,
	5512,
	8000,
	9600,
	11025,
	16000,
	22050,
	32000,
	44100,
	48000,
	88200,
	96000,
	176400,
	192000
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

enum airtaudio::errorType airtaudio::Api::openStream(airtaudio::StreamParameters *oParams,
                                                     airtaudio::StreamParameters *iParams,
                                                     enum audio::format format,
                                                     uint32_t sampleRate,
                                                     uint32_t *bufferFrames,
                                                     airtaudio::AirTAudioCallback callback,
                                                     airtaudio::StreamOptions *options) {
	if (m_stream.state != airtaudio::api::STREAM_CLOSED) {
		ATA_ERROR("a stream is already open!");
		return airtaudio::errorInvalidUse;
	}
	if (oParams && oParams->nChannels < 1) {
		ATA_ERROR("a non-nullptr output StreamParameters structure cannot have an nChannels value less than one.");
		return airtaudio::errorInvalidUse;
	}
	if (iParams && iParams->nChannels < 1) {
		ATA_ERROR("a non-nullptr input StreamParameters structure cannot have an nChannels value less than one.");
		return airtaudio::errorInvalidUse;
	}
	if (oParams == nullptr && iParams == nullptr) {
		ATA_ERROR("input and output StreamParameters structures are both nullptr!");
		return airtaudio::errorInvalidUse;
	}
	if (audio::getFormatBytes(format) == 0) {
		ATA_ERROR("'format' parameter value is undefined.");
		return airtaudio::errorInvalidUse;
	}
	uint32_t nDevices = getDeviceCount();
	uint32_t oChannels = 0;
	if (oParams) {
		oChannels = oParams->nChannels;
		if (oParams->deviceId >= nDevices) {
			ATA_ERROR("output device parameter value is invalid.");
			return airtaudio::errorInvalidUse;
		}
	}
	uint32_t iChannels = 0;
	if (iParams) {
		iChannels = iParams->nChannels;
		if (iParams->deviceId >= nDevices) {
			ATA_ERROR("input device parameter value is invalid.");
			return airtaudio::errorInvalidUse;
		}
	}
	clearStreamInfo();
	bool result;
	if (oChannels > 0) {
		result = probeDeviceOpen(oParams->deviceId,
		                         airtaudio::api::OUTPUT,
		                         oChannels,
		                         oParams->firstChannel,
		                         sampleRate,
		                         format,
		                         bufferFrames,
		                         options);
		if (result == false) {
			ATA_ERROR("system ERROR");
			return airtaudio::errorSystemError;
		}
	}
	if (iChannels > 0) {
		result = probeDeviceOpen(iParams->deviceId,
		                         airtaudio::api::INPUT,
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
			return airtaudio::errorSystemError;
		}
	}
	m_stream.callbackInfo.callback = callback;
	if (options != nullptr) {
		options->numberOfBuffers = m_stream.nBuffers;
	}
	m_stream.state = airtaudio::api::STREAM_STOPPED;
	return airtaudio::errorNone;
}

uint32_t airtaudio::Api::getDefaultInputDevice() {
	// Should be implemented in subclasses if possible.
	return 0;
}

uint32_t airtaudio::Api::getDefaultOutputDevice() {
	// Should be implemented in subclasses if possible.
	return 0;
}

enum airtaudio::errorType airtaudio::Api::closeStream() {
	// MUST be implemented in subclasses!
	return airtaudio::errorNone;
}

bool airtaudio::Api::probeDeviceOpen(uint32_t /*device*/,
                                     airtaudio::api::StreamMode /*mode*/,
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
	if (verifyStream() != airtaudio::errorNone) {
		return 0;
	}
	long totalLatency = 0;
	if (    m_stream.mode == airtaudio::api::OUTPUT
	     || m_stream.mode == airtaudio::api::DUPLEX) {
		totalLatency = m_stream.latency[0];
	}
	if (    m_stream.mode == airtaudio::api::INPUT
	     || m_stream.mode == airtaudio::api::DUPLEX) {
		totalLatency += m_stream.latency[1];
	}
	return totalLatency;
}

double airtaudio::Api::getStreamTime() {
	if (verifyStream() != airtaudio::errorNone) {
		return 0.0f;
	}
#if defined(HAVE_GETTIMEOFDAY)
	// Return a very accurate estimate of the stream time by
	// adding in the elapsed time since the last tick.
	struct timeval then;
	struct timeval now;
	if (m_stream.state != airtaudio::api::STREAM_RUNNING || m_stream.streamTime == 0.0) {
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
	if (verifyStream() != airtaudio::errorNone) {
		return 0;
	}
	return m_stream.sampleRate;
}

enum airtaudio::errorType airtaudio::Api::verifyStream() {
	if (m_stream.state == airtaudio::api::STREAM_CLOSED) {
		ATA_ERROR("a stream is not open!");
		return airtaudio::errorInvalidUse;
	}
	return airtaudio::errorNone;
}

void airtaudio::Api::clearStreamInfo() {
	m_stream.mode = airtaudio::api::UNINITIALIZED;
	m_stream.state = airtaudio::api::STREAM_CLOSED;
	m_stream.sampleRate = 0;
	m_stream.bufferSize = 0;
	m_stream.nBuffers = 0;
	m_stream.userFormat = audio::format_unknow;
	m_stream.userInterleaved = true;
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

void airtaudio::Api::setConvertInfo(airtaudio::api::StreamMode _mode, uint32_t _firstChannel) {
	if (_mode == airtaudio::api::INPUT) { // convert device to user buffer
		m_stream.convertInfo[_mode].inJump = m_stream.nDeviceChannels[1];
		m_stream.convertInfo[_mode].outJump = m_stream.nUserChannels[1];
		m_stream.convertInfo[_mode].inFormat = m_stream.deviceFormat[1];
		m_stream.convertInfo[_mode].outFormat = m_stream.userFormat;
	} else { // convert user to device buffer
		m_stream.convertInfo[_mode].inJump = m_stream.nUserChannels[0];
		m_stream.convertInfo[_mode].outJump = m_stream.nDeviceChannels[0];
		m_stream.convertInfo[_mode].inFormat = m_stream.userFormat;
		m_stream.convertInfo[_mode].outFormat = m_stream.deviceFormat[0];
	}
	if (m_stream.convertInfo[_mode].inJump < m_stream.convertInfo[_mode].outJump) {
		m_stream.convertInfo[_mode].channels = m_stream.convertInfo[_mode].inJump;
	} else {
		m_stream.convertInfo[_mode].channels = m_stream.convertInfo[_mode].outJump;
	}
	// Set up the interleave/deinterleave offsets.
	if (m_stream.deviceInterleaved[_mode] != m_stream.userInterleaved) {
		if (    (    _mode == airtaudio::api::OUTPUT
		          && m_stream.deviceInterleaved[_mode])
		     || (    _mode == airtaudio::api::INPUT
		          && m_stream.userInterleaved)) {
			for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
				m_stream.convertInfo[_mode].inOffset.push_back(kkk * m_stream.bufferSize);
				m_stream.convertInfo[_mode].outOffset.push_back(kkk);
				m_stream.convertInfo[_mode].inJump = 1;
			}
		} else {
			for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
				m_stream.convertInfo[_mode].inOffset.push_back(kkk);
				m_stream.convertInfo[_mode].outOffset.push_back(kkk * m_stream.bufferSize);
				m_stream.convertInfo[_mode].outJump = 1;
			}
		}
	} else { // no (de)interleaving
		if (m_stream.userInterleaved) {
			for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
				m_stream.convertInfo[_mode].inOffset.push_back(kkk);
				m_stream.convertInfo[_mode].outOffset.push_back(kkk);
			}
		} else {
			for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
				m_stream.convertInfo[_mode].inOffset.push_back(kkk * m_stream.bufferSize);
				m_stream.convertInfo[_mode].outOffset.push_back(kkk * m_stream.bufferSize);
				m_stream.convertInfo[_mode].inJump = 1;
				m_stream.convertInfo[_mode].outJump = 1;
			}
		}
	}

	// Add channel offset.
	if (_firstChannel > 0) {
		if (m_stream.deviceInterleaved[_mode]) {
			if (_mode == airtaudio::api::OUTPUT) {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
					m_stream.convertInfo[_mode].outOffset[kkk] += _firstChannel;
				}
			} else {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
					m_stream.convertInfo[_mode].inOffset[kkk] += _firstChannel;
				}
			}
		} else {
			if (_mode == airtaudio::api::OUTPUT) {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
					m_stream.convertInfo[_mode].outOffset[kkk] += (_firstChannel * m_stream.bufferSize);
				}
			} else {
				for (int32_t kkk=0; kkk<m_stream.convertInfo[_mode].channels; ++kkk) {
					m_stream.convertInfo[_mode].inOffset[kkk] += (_firstChannel * m_stream.bufferSize);
				}
			}
		}
	}
}

void airtaudio::Api::convertBuffer(char *_outBuffer, char *_inBuffer, airtaudio::api::ConvertInfo &_info) {
	// This function does format conversion, input/output channel compensation, and
	// data interleaving/deinterleaving.	24-bit integers are assumed to occupy
	// the lower three bytes of a 32-bit integer.

	// Clear our device buffer when in/out duplex device channels are different
	if (    _outBuffer == m_stream.deviceBuffer
	     && m_stream.mode == airtaudio::api::DUPLEX
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


