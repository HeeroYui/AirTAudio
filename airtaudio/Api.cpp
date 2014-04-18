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



etk::CCout& operator <<(etk::CCout& _os, const airtaudio::api::type& _obj){
	switch (_obj) {
		default:
		case airtaudio::api::UNSPECIFIED: _os << "UNSPECIFIED"; break;
		case airtaudio::api::LINUX_ALSA: _os << "LINUX_ALSA"; break;
		case airtaudio::api::LINUX_PULSE: _os << "LINUX_PULSE"; break;
		case airtaudio::api::LINUX_OSS: _os << "LINUX_OSS"; break;
		case airtaudio::api::UNIX_JACK: _os << "UNIX_JACK"; break;
		case airtaudio::api::MACOSX_CORE: _os << "MACOSX_CORE"; break;
		case airtaudio::api::WINDOWS_ASIO: _os << "WINDOWS_ASIO"; break;
		case airtaudio::api::WINDOWS_DS: _os << "WINDOWS_DS"; break;
		case airtaudio::api::RTAUDIO_DUMMY: _os << "RTAUDIO_DUMMY"; break;
		case airtaudio::api::ANDROID_JAVA: _os << "ANDROID_JAVA"; break;
		case airtaudio::api::USER_INTERFACE_1: _os << "USER_INTERFACE_1"; break;
		case airtaudio::api::USER_INTERFACE_2: _os << "USER_INTERFACE_2"; break;
		case airtaudio::api::USER_INTERFACE_3: _os << "USER_INTERFACE_3"; break;
		case airtaudio::api::USER_INTERFACE_4: _os << "USER_INTERFACE_4"; break;
	}
	return _os;
}

// Static variable definitions.
const uint32_t airtaudio::api::MAX_SAMPLE_RATES = 14;
const uint32_t airtaudio::api::SAMPLE_RATES[] = {
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


airtaudio::Api::Api(void) {
	m_stream.state = airtaudio::api::STREAM_CLOSED;
	m_stream.mode = airtaudio::api::UNINITIALIZED;
	m_stream.apiHandle = 0;
	m_stream.userBuffer[0] = 0;
	m_stream.userBuffer[1] = 0;
}

airtaudio::Api::~Api(void) {
	
}

enum airtaudio::errorType airtaudio::Api::openStream(airtaudio::StreamParameters *oParams,
                                                     airtaudio::StreamParameters *iParams,
                                                     airtaudio::format format,
                                                     uint32_t sampleRate,
                                                     uint32_t *bufferFrames,
                                                     airtaudio::AirTAudioCallback callback,
                                                     void *userData,
                                                     airtaudio::StreamOptions *options) {
	if (m_stream.state != airtaudio::api::STREAM_CLOSED) {
		ATA_ERROR("airtaudio::Api::openStream: a stream is already open!");
		return airtaudio::errorInvalidUse;
	}
	if (oParams && oParams->nChannels < 1) {
		ATA_ERROR("airtaudio::Api::openStream: a non-NULL output StreamParameters structure cannot have an nChannels value less than one.");
		return airtaudio::errorInvalidUse;
	}
	if (iParams && iParams->nChannels < 1) {
		ATA_ERROR("airtaudio::Api::openStream: a non-NULL input StreamParameters structure cannot have an nChannels value less than one.");
		return airtaudio::errorInvalidUse;
	}
	if (oParams == NULL && iParams == NULL) {
		ATA_ERROR("airtaudio::Api::openStream: input and output StreamParameters structures are both NULL!");
		return airtaudio::errorInvalidUse;
	}
	if (formatBytes(format) == 0) {
		ATA_ERROR("airtaudio::Api::openStream: 'format' parameter value is undefined.");
		return airtaudio::errorInvalidUse;
	}
	uint32_t nDevices = getDeviceCount();
	uint32_t oChannels = 0;
	if (oParams) {
		oChannels = oParams->nChannels;
		if (oParams->deviceId >= nDevices) {
			ATA_ERROR("airtaudio::Api::openStream: output device parameter value is invalid.");
			return airtaudio::errorInvalidUse;
		}
	}
	uint32_t iChannels = 0;
	if (iParams) {
		iChannels = iParams->nChannels;
		if (iParams->deviceId >= nDevices) {
			ATA_ERROR("airtaudio::Api::openStream: input device parameter value is invalid.");
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
	m_stream.callbackInfo.callback = (void *) callback;
	m_stream.callbackInfo.userData = userData;
	if (options != NULL) {
		options->numberOfBuffers = m_stream.nBuffers;
	}
	m_stream.state = airtaudio::api::STREAM_STOPPED;
	return airtaudio::errorNone;
}

uint32_t airtaudio::Api::getDefaultInputDevice(void) {
	// Should be implemented in subclasses if possible.
	return 0;
}

uint32_t airtaudio::Api::getDefaultOutputDevice(void) {
	// Should be implemented in subclasses if possible.
	return 0;
}

enum airtaudio::errorType airtaudio::Api::closeStream(void) {
	// MUST be implemented in subclasses!
	return airtaudio::errorNone;
}

bool airtaudio::Api::probeDeviceOpen(uint32_t /*device*/,
                                     airtaudio::api::StreamMode /*mode*/,
                                     uint32_t /*channels*/,
                                     uint32_t /*firstChannel*/,
                                     uint32_t /*sampleRate*/,
                                     airtaudio::format /*format*/,
                                     uint32_t * /*bufferSize*/,
                                     airtaudio::StreamOptions * /*options*/) {
	// MUST be implemented in subclasses!
	return false;
}

void airtaudio::Api::tickStreamTime(void) {
	// Subclasses that do not provide their own implementation of
	// getStreamTime should call this function once per buffer I/O to
	// provide basic stream time support.
	m_stream.streamTime += (m_stream.bufferSize * 1.0 / m_stream.sampleRate);
#if defined(HAVE_GETTIMEOFDAY)
	gettimeofday(&m_stream.lastTickTimestamp, NULL);
#endif
}

long airtaudio::Api::getStreamLatency(void) {
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

double airtaudio::Api::getStreamTime(void) {
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
	gettimeofday(&now, NULL);
	then = m_stream.lastTickTimestamp;
	return   m_stream.streamTime
	       + ((now.tv_sec + 0.000001 * now.tv_usec)
	       - (then.tv_sec + 0.000001 * then.tv_usec));
#else
	return m_stream.streamTime;
#endif
}

uint32_t airtaudio::Api::getStreamSampleRate(void) {
	if (verifyStream() != airtaudio::errorNone) {
		return 0;
	}
	return m_stream.sampleRate;
}

enum airtaudio::errorType airtaudio::Api::verifyStream(void) {
	if (m_stream.state == airtaudio::api::STREAM_CLOSED) {
		ATA_ERROR("airtaudio::Api:: a stream is not open!");
		return airtaudio::errorInvalidUse;
	}
	return airtaudio::errorNone;
}

void airtaudio::Api::clearStreamInfo(void) {
	m_stream.mode = airtaudio::api::UNINITIALIZED;
	m_stream.state = airtaudio::api::STREAM_CLOSED;
	m_stream.sampleRate = 0;
	m_stream.bufferSize = 0;
	m_stream.nBuffers = 0;
	m_stream.userFormat = 0;
	m_stream.userInterleaved = true;
	m_stream.streamTime = 0.0;
	m_stream.apiHandle = 0;
	m_stream.deviceBuffer = 0;
	m_stream.callbackInfo.callback = 0;
	m_stream.callbackInfo.userData = 0;
	m_stream.callbackInfo.isRunning = false;
	for (int32_t iii=0; iii<2; ++iii) {
		m_stream.device[iii] = 11111;
		m_stream.doConvertBuffer[iii] = false;
		m_stream.deviceInterleaved[iii] = true;
		m_stream.doByteSwap[iii] = false;
		m_stream.nUserChannels[iii] = 0;
		m_stream.nDeviceChannels[iii] = 0;
		m_stream.channelOffset[iii] = 0;
		m_stream.deviceFormat[iii] = 0;
		m_stream.latency[iii] = 0;
		m_stream.userBuffer[iii] = 0;
		m_stream.convertInfo[iii].channels = 0;
		m_stream.convertInfo[iii].inJump = 0;
		m_stream.convertInfo[iii].outJump = 0;
		m_stream.convertInfo[iii].inFormat = 0;
		m_stream.convertInfo[iii].outFormat = 0;
		m_stream.convertInfo[iii].inOffset.clear();
		m_stream.convertInfo[iii].outOffset.clear();
	}
}

uint32_t airtaudio::Api::formatBytes(airtaudio::format _format)
{
	if (_format == airtaudio::SINT16) {
		return 2;
	} else if (    _format == airtaudio::SINT32
	            || _format == airtaudio::FLOAT32) {
		return 4;
	} else if (_format == airtaudio::FLOAT64) {
		return 8;
	} else if (_format == airtaudio::SINT24) {
		return 3;
	} else if (_format == airtaudio::SINT8) {
		return 1;
	}
	ATA_ERROR("airtaudio::Api::formatBytes: undefined format.");
	// TODO : airtaudio::errorWarning;
	return 0;
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
		memset(_outBuffer, 0, m_stream.bufferSize * _info.outJump * formatBytes(_info.outFormat));
	}
	int32_t jjj;
	if (_info.outFormat == airtaudio::FLOAT64) {
		double scale;
		double *out = (double *)_outBuffer;

		if (_info.inFormat == airtaudio::SINT8) {
			signed char *in = (signed char *)_inBuffer;
			scale = 1.0 / 127.5;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (double) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT16) {
			int16_t *in = (int16_t *)_inBuffer;
			scale = 1.0 / 32767.5;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (double) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT24) {
			int24_t  *in = (int24_t  *)_inBuffer;
			scale = 1.0 / 8388607.5;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (double) (in[_info.inOffset[jjj]].asInt());
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT32) {
			int32_t *in = (int32_t *)_inBuffer;
			scale = 1.0 / 2147483647.5;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (double) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT32) {
			float *in = (float *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (double) in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT64) {
			// Channel compensation and/or (de)interleaving only.
			double *in = (double *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
	}
	else if (_info.outFormat == airtaudio::FLOAT32) {
		float scale;
		float *out = (float *)_outBuffer;
		if (_info.inFormat == airtaudio::SINT8) {
			signed char *in = (signed char *)_inBuffer;
			scale = (float) (1.0 / 127.5);
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (float) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT16) {
			int16_t *in = (int16_t *)_inBuffer;
			scale = (float) (1.0 / 32767.5);
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (float) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT24) {
			int24_t  *in = (int24_t  *)_inBuffer;
			scale = (float) (1.0 / 8388607.5);
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (float) (in[_info.inOffset[jjj]].asInt());
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT32) {
			int32_t *in = (int32_t *)_inBuffer;
			scale = (float) (1.0 / 2147483647.5);
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (float) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] += 0.5;
					out[_info.outOffset[jjj]] *= scale;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT32) {
			// Channel compensation and/or (de)interleaving only.
			float *in = (float *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT64) {
			double *in = (double *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (float) in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
	}
	else if (_info.outFormat == airtaudio::SINT32) {
		int32_t *out = (int32_t *)_outBuffer;
		if (_info.inFormat == airtaudio::SINT8) {
			signed char *in = (signed char *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] <<= 24;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT16) {
			int16_t *in = (int16_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] <<= 16;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT24) {
			int24_t  *in = (int24_t  *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) in[_info.inOffset[jjj]].asInt();
					out[_info.outOffset[jjj]] <<= 8;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT32) {
			// Channel compensation and/or (de)interleaving only.
			int32_t *in = (int32_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT32) {
			float *in = (float *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) (in[_info.inOffset[jjj]] * 2147483647.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT64) {
			double *in = (double *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) (in[_info.inOffset[jjj]] * 2147483647.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
	}
	else if (_info.outFormat == airtaudio::SINT24) {
		int24_t  *out = (int24_t  *)_outBuffer;
		if (_info.inFormat == airtaudio::SINT8) {
			signed char *in = (signed char *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) (in[_info.inOffset[jjj]] << 16);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT16) {
			int16_t *in = (int16_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) (in[_info.inOffset[jjj]] << 8);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT24) {
			// Channel compensation and/or (de)interleaving only.
			int24_t  *in = (int24_t  *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT32) {
			int32_t *in = (int32_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) (in[_info.inOffset[jjj]] >> 8);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT32) {
			float *in = (float *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) (in[_info.inOffset[jjj]] * 8388607.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT64) {
			double *in = (double *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int32_t) (in[_info.inOffset[jjj]] * 8388607.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
	}
	else if (_info.outFormat == airtaudio::SINT16) {
		int16_t *out = (int16_t *)_outBuffer;
		if (_info.inFormat == airtaudio::SINT8) {
			signed char *in = (signed char *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int16_t) in[_info.inOffset[jjj]];
					out[_info.outOffset[jjj]] <<= 8;
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT16) {
			// Channel compensation and/or (de)interleaving only.
			int16_t *in = (int16_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT24) {
			int24_t  *in = (int24_t  *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int16_t) (in[_info.inOffset[jjj]].asInt() >> 8);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT32) {
			int32_t *in = (int32_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int16_t) ((in[_info.inOffset[jjj]] >> 16) & 0x0000ffff);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT32) {
			float *in = (float *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int16_t) (in[_info.inOffset[jjj]] * 32767.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT64) {
			double *in = (double *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (int16_t) (in[_info.inOffset[jjj]] * 32767.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
	}
	else if (_info.outFormat == airtaudio::SINT8) {
		signed char *out = (signed char *)_outBuffer;
		if (_info.inFormat == airtaudio::SINT8) {
			// Channel compensation and/or (de)interleaving only.
			signed char *in = (signed char *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = in[_info.inOffset[jjj]];
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		if (_info.inFormat == airtaudio::SINT16) {
			int16_t *in = (int16_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (signed char) ((in[_info.inOffset[jjj]] >> 8) & 0x00ff);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT24) {
			int24_t  *in = (int24_t  *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (signed char) (in[_info.inOffset[jjj]].asInt() >> 16);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::SINT32) {
			int32_t *in = (int32_t *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (signed char) ((in[_info.inOffset[jjj]] >> 24) & 0x000000ff);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT32) {
			float *in = (float *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (signed char) (in[_info.inOffset[jjj]] * 127.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
		else if (_info.inFormat == airtaudio::FLOAT64) {
			double *in = (double *)_inBuffer;
			for (uint32_t iii=0; iii<m_stream.bufferSize; ++iii) {
				for (jjj=0; jjj<_info.channels; ++jjj) {
					out[_info.outOffset[jjj]] = (signed char) (in[_info.inOffset[jjj]] * 127.5 - 0.5);
				}
				in += _info.inJump;
				out += _info.outJump;
			}
		}
	}
}

void airtaudio::Api::byteSwapBuffer(char *_buffer, uint32_t _samples, airtaudio::format _format) {
	register char val;
	register char *ptr;
	ptr = _buffer;
	if (_format == airtaudio::SINT16) {
		for (uint32_t iii=0; iii<_samples; ++iii) {
			// Swap 1st and 2nd bytes.
			val = *(ptr);
			*(ptr) = *(ptr+1);
			*(ptr+1) = val;

			// Increment 2 bytes.
			ptr += 2;
		}
	} else if (    _format == airtaudio::SINT32
	            || _format == airtaudio::FLOAT32) {
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
	} else if (_format == airtaudio::SINT24) {
		for (uint32_t iii=0; iii<_samples; ++iii) {
			// Swap 1st and 3rd bytes.
			val = *(ptr);
			*(ptr) = *(ptr+2);
			*(ptr+2) = val;

			// Increment 2 more bytes.
			ptr += 2;
		}
	} else if (_format == airtaudio::FLOAT64) {
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


