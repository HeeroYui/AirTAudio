/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#include <airtaudio/Interface.h>
#include <iostream>

std::vector<airtaudio::api::type> airtaudio::Interface::getCompiledApi(void) {
	std::vector<airtaudio::api::type> apis;
	// The order here will control the order of RtAudio's API search in
	// the constructor.
	for (auto &it : m_apiAvaillable) {
		apis.push_back(it.first);
	}
	return apis;
}



void airtaudio::Interface::openRtApi(airtaudio::api::type _api) {
	if (m_rtapi != NULL) {
		delete m_rtapi;
		m_rtapi = NULL;
	}
	for (auto &it :m_apiAvaillable) {
		if (_api == it.first) {
			m_rtapi = it.second();
			if (m_rtapi != NULL) {
				return;
			}
		}
	}
	// TODO : An eror occured ...
}


airtaudio::Interface::Interface(void) :
  m_rtapi(NULL) {
#if defined(__UNIX_JACK__)
	addInterface(airtaudio::api::UNIX_JACK, airtaudio::api::Jack::Create);
#endif
#if defined(__LINUX_ALSA__)
	addInterface(airtaudio::api::LINUX_ALSA, airtaudio::api::Alsa::Create);
#endif
#if defined(__LINUX_PULSE__)
	addInterface(airtaudio::api::LINUX_PULSE, airtaudio::api::Pulse::Create);
#endif
#if defined(__LINUX_OSS__)
	addInterface(airtaudio::api::LINUX_OSS, airtaudio::api::Oss::Create);
#endif
#if defined(__WINDOWS_ASIO__)
	addInterface(airtaudio::api::WINDOWS_ASIO, airtaudio::api::Asio::Create);
#endif
#if defined(__WINDOWS_DS__)
	addInterface(airtaudio::api::WINDOWS_DS, airtaudio::api::Ds::Create);
#endif
#if defined(__MACOSX_CORE__)
	addInterface(airtaudio::api::MACOSX_CORE, airtaudio::api::Core::Create);
#endif
#if defined(__AIRTAUDIO_DUMMY__)
	addInterface(airtaudio::api::RTAUDIO_DUMMY, airtaudio::api::Dummy::Create);
#endif
}

void airtaudio::Interface::addInterface(airtaudio::api::type _api, Api* (*_callbackCreate)(void)) {
	m_apiAvaillable.push_back(std::pair<airtaudio::api::type, Api* (*)(void)>(_api, _callbackCreate));
}

enum airtaudio::errorType airtaudio::Interface::instanciate(airtaudio::api::type _api) {
	if (m_rtapi != NULL) {
		std::cerr << "\nInterface already started ...!\n" << std::endl;
		return airtaudio::errorNone;
	}
	if (_api != airtaudio::api::UNSPECIFIED) {
		// Attempt to open the specified API.
		openRtApi(_api);
		if (m_rtapi != NULL) {
			return airtaudio::errorNone;
		}
		// No compiled support for specified API value.	Issue a debug
		// warning and continue as if no API was specified.
		std::cerr << "\nRtAudio: no compiled support for specified API argument!\n" << std::endl;
		return airtaudio::errorFail;
	}
	// Iterate through the compiled APIs and return as soon as we find
	// one with at least one device or we reach the end of the list.
	std::vector<airtaudio::api::type> apis = getCompiledApi();
	for (auto &it : apis) {
		openRtApi(it);
		if (m_rtapi->getDeviceCount() != 0) {
			break;
		}
	}
	if (m_rtapi != NULL) {
		return airtaudio::errorNone;
	}
	std::cout << "\nRtAudio: no compiled API support found ... critical error!!\n\n";
	return airtaudio::errorFail;
}

airtaudio::Interface::~Interface(void) {
	if (m_rtapi != NULL) {
		delete m_rtapi;
		m_rtapi = NULL;
	}
}

enum airtaudio::errorType airtaudio::Interface::openStream(
                airtaudio::StreamParameters* _outputParameters,
                airtaudio::StreamParameters* _inputParameters,
                airtaudio::format _format,
                uint32_t _sampleRate,
                uint32_t* _bufferFrames,
                airtaudio::AirTAudioCallback _callback,
                void* _userData,
                airtaudio::StreamOptions* _options,
                airtaudio::AirTAudioErrorCallback _errorCallback) {
	if (m_rtapi == NULL) {
		return;
	}
	return m_rtapi->openStream(_outputParameters,
	                           _inputParameters,
	                           _format,
	                           _sampleRate,
	                           _bufferFrames,
	                           _callback,
	                           _userData,
	                           _options,
	                           _errorCallback);
}




