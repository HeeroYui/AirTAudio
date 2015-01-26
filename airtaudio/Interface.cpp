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

std::vector<airtaudio::api::type> airtaudio::Interface::getCompiledApi() {
	std::vector<airtaudio::api::type> apis;
	// The order here will control the order of RtAudio's API search in
	// the constructor.
	for (auto &it : m_apiAvaillable) {
		apis.push_back(it.first);
	}
	return apis;
}



void airtaudio::Interface::openRtApi(airtaudio::api::type _api) {
	
	delete m_rtapi;
	m_rtapi = nullptr;
	for (auto &it :m_apiAvaillable) {
		ATA_ERROR("try open " << it.first);
		if (_api == it.first) {
			ATA_ERROR("    ==> call it");
			m_rtapi = it.second();
			if (m_rtapi != nullptr) {
				return;
			}
		}
	}
	// TODO : An eror occured ...
	ATA_ERROR("Error in open API ...");
}


airtaudio::Interface::Interface() :
  m_rtapi(nullptr) {
	ATA_DEBUG("Add interface:");
#if defined(__UNIX_JACK__)
	ATA_DEBUG("    JACK");
	addInterface(airtaudio::api::UNIX_JACK, airtaudio::api::Jack::Create);
#endif
#if defined(__LINUX_ALSA__)
	ATA_DEBUG("    ALSA");
	addInterface(airtaudio::api::LINUX_ALSA, airtaudio::api::Alsa::Create);
#endif
#if defined(__LINUX_PULSE__)
	ATA_DEBUG("    PULSE");
	addInterface(airtaudio::api::LINUX_PULSE, airtaudio::api::Pulse::Create);
#endif
#if defined(__LINUX_OSS__)
	ATA_DEBUG("    OSS");
	addInterface(airtaudio::api::LINUX_OSS, airtaudio::api::Oss::Create);
#endif
#if defined(__WINDOWS_ASIO__)
	ATA_DEBUG("    ASIO");
	addInterface(airtaudio::api::WINDOWS_ASIO, airtaudio::api::Asio::Create);
#endif
#if defined(__WINDOWS_DS__)
	ATA_DEBUG("    DS");
	addInterface(airtaudio::api::WINDOWS_DS, airtaudio::api::Ds::Create);
#endif
#if defined(__MACOSX_CORE__)
	ATA_DEBUG("    MACOSX_CORE");
	addInterface(airtaudio::api::MACOSX_CORE, airtaudio::api::Core::Create);
#endif
#if defined(__IOS_CORE__)
	ATA_DEBUG("    IOS_CORE");
	addInterface(airtaudio::api::IOS_CORE, airtaudio::api::CoreIos::Create);
#endif
#if defined(__ANDROID_JAVA__)
	ATA_DEBUG("    JAVA");
	addInterface(airtaudio::api::ANDROID_JAVA, airtaudio::api::Android::Create);
#endif
#if defined(__AIRTAUDIO_DUMMY__)
	ATA_DEBUG("    DUMMY");
	addInterface(airtaudio::api::RTAUDIO_DUMMY, airtaudio::api::Dummy::Create);
#endif
}

void airtaudio::Interface::addInterface(airtaudio::api::type _api, Api* (*_callbackCreate)()) {
	m_apiAvaillable.push_back(std::pair<airtaudio::api::type, Api* (*)()>(_api, _callbackCreate));
}

enum airtaudio::errorType airtaudio::Interface::instanciate(airtaudio::api::type _api) {
	ATA_INFO("Instanciate API ...");
	if (m_rtapi != nullptr) {
		ATA_WARNING("Interface already started ...!");
		return airtaudio::errorNone;
	}
	if (_api != airtaudio::api::UNSPECIFIED) {
		ATA_ERROR("API specified ...");
		// Attempt to open the specified API.
		openRtApi(_api);
		if (m_rtapi != nullptr) {
			return airtaudio::errorNone;
		}
		// No compiled support for specified API value.	Issue a debug
		// warning and continue as if no API was specified.
		ATA_ERROR("RtAudio: no compiled support for specified API argument!");
		return airtaudio::errorFail;
	}
	ATA_INFO("Auto choice API :");
	// Iterate through the compiled APIs and return as soon as we find
	// one with at least one device or we reach the end of the list.
	std::vector<airtaudio::api::type> apis = getCompiledApi();
	ATA_INFO(" find : " << apis.size() << " apis.");
	for (auto &it : apis) {
		ATA_INFO("try open ...");
		openRtApi(it);
		if(m_rtapi == nullptr) {
			ATA_ERROR("    ==> can not create ...");
			continue;
		}
		if (m_rtapi->getDeviceCount() != 0) {
			ATA_INFO("    ==> api open");
			break;
		}
	}
	if (m_rtapi != nullptr) {
		return airtaudio::errorNone;
	}
	ATA_ERROR("RtAudio: no compiled API support found ... critical error!!");
	return airtaudio::errorFail;
}

airtaudio::Interface::~Interface() {
	ATA_INFO("Remove interface");
	delete m_rtapi;
	m_rtapi = nullptr;
}

enum airtaudio::errorType airtaudio::Interface::openStream(
                airtaudio::StreamParameters* _outputParameters,
                airtaudio::StreamParameters* _inputParameters,
                airtaudio::format _format,
                uint32_t _sampleRate,
                uint32_t* _bufferFrames,
                airtaudio::AirTAudioCallback _callback,
                void* _userData,
                airtaudio::StreamOptions* _options) {
	if (m_rtapi == nullptr) {
		return airtaudio::errorInputNull;
	}
	return m_rtapi->openStream(_outputParameters,
	                           _inputParameters,
	                           _format,
	                           _sampleRate,
	                           _bufferFrames,
	                           _callback,
	                           _userData,
	                           _options);
}




