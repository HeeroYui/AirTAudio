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
#if defined(__UNIX_JACK__)
	apis.push_back(airtaudio::api::UNIX_JACK);
#endif
#if defined(__LINUX_ALSA__)
	apis.push_back(airtaudio::api::LINUX_ALSA);
#endif
#if defined(__LINUX_PULSE__)
	apis.push_back(airtaudio::api::LINUX_PULSE);
#endif
#if defined(__LINUX_OSS__)
	apis.push_back(airtaudio::api::LINUX_OSS);
#endif
#if defined(__WINDOWS_ASIO__)
	apis.push_back(airtaudio::api::WINDOWS_ASIO);
#endif
#if defined(__WINDOWS_DS__)
	apis.push_back(airtaudio::api::WINDOWS_DS);
#endif
#if defined(__MACOSX_CORE__)
	apis.push_back(airtaudio::api::MACOSX_CORE);
#endif
#if defined(__AIRTAUDIO_DUMMY__)
	apis.push_back(airtaudio::api::RTAUDIO_DUMMY);
#endif
	return apis;
}

void airtaudio::Interface::openRtApi(airtaudio::api::type _api) {
	if (m_rtapi != NULL) {
		delete m_rtapi;
		m_rtapi = NULL;
	}
#if defined(__UNIX_JACK__)
	if (_api == airtaudio::api::UNIX_JACK) {
		m_rtapi = new airtaudio::api::Jack();
	}
#endif
#if defined(__LINUX_ALSA__)
	if (_api == airtaudio::api::LINUX_ALSA) {
		m_rtapi = new airtaudio::api::Alsa();
	}
#endif
#if defined(__LINUX_PULSE__)
	if (_api == airtaudio::api::LINUX_PULSE) {
		m_rtapi = new airtaudio::api::Pulse();
	}
#endif
#if defined(__LINUX_OSS__)
	if (_api == airtaudio::api::LINUX_OSS) {
		m_rtapi = new airtaudio::api::Oss();
	}
#endif
#if defined(__WINDOWS_ASIO__)
	if (_api == airtaudio::api::WINDOWS_ASIO) {
		m_rtapi = new airtaudio::api::Asio();
	}
#endif
#if defined(__WINDOWS_DS__)
	if (_api == airtaudio::api::WINDOWS_DS) {
		m_rtapi = new airtaudio::api::Ds();
	}
#endif
#if defined(__MACOSX_CORE__)
	if (_api == airtaudio::api::MACOSX_CORE) {
		m_rtapi = new airtaudio::api::Core();
	}
#endif
#if defined(__AIRTAUDIO_DUMMY__)
	if (_api == rtaudio::RTAUDIO_DUMMY) {
		m_rtapi = new airtaudio::api::Dummy();
	}
#endif
}

airtaudio::Interface::Interface(airtaudio::api::type _api) :
  m_rtapi(NULL) {
	if (_api != airtaudio::api::UNSPECIFIED) {
		// Attempt to open the specified API.
		openRtApi(_api);
		if (m_rtapi != NULL) {
			return;
		}
		// No compiled support for specified API value.	Issue a debug
		// warning and continue as if no API was specified.
		std::cerr << "\nRtAudio: no compiled support for specified API argument!\n" << std::endl;
	}
	// Iterate through the compiled APIs and return as soon as we find
	// one with at least one device or we reach the end of the list.
	std::vector<airtaudio::api::type> apis = getCompiledApi();
	for (uint32_t iii=0; iii<apis.size(); ++iii) {
		openRtApi(apis[iii]);
		if (m_rtapi->getDeviceCount() != 0) {
			break;
		}
	}
	if (m_rtapi != NULL) {
		return;
	}
	// It should not be possible to get here because the preprocessor
	// definition __AIRTAUDIO_DUMMY__ is automatically defined if no
	// API-specific definitions are passed to the compiler. But just in
	// case something weird happens, we'll print out an error message.
	// TODO : Set it in error ...
	std::cout << "\nRtAudio: no compiled API support found ... critical error!!\n\n";
}

airtaudio::Interface::~Interface(void) {
	if (m_rtapi != NULL) {
		delete m_rtapi;
		m_rtapi = NULL;
	}
}

void airtaudio::Interface::openStream(airtaudio::StreamParameters* _outputParameters,
                                      airtaudio::StreamParameters* _inputParameters,
                                      airtaudio::format _format,
                                      uint32_t _sampleRate,
                                      uint32_t* _bufferFrames,
                                      airtaudio::AirTAudioCallback _callback,
                                      void* _userData,
                                      airtaudio::StreamOptions* _options,
                                      airtaudio::AirTAudioErrorCallback _errorCallback)
{
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




