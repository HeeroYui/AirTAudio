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

#undef __class__
#define __class__ "Interface"

std::vector<enum airtaudio::type> airtaudio::Interface::getCompiledApi() {
	std::vector<enum airtaudio::type> apis;
	// The order here will control the order of RtAudio's API search in
	// the constructor.
	for (size_t iii=0; iii<m_apiAvaillable.size(); ++iii) {
		apis.push_back(m_apiAvaillable[iii].first);
	}
	return apis;
}



void airtaudio::Interface::openRtApi(enum airtaudio::type _api) {
	delete m_rtapi;
	m_rtapi = nullptr;
	for (size_t iii=0; iii<m_apiAvaillable.size(); ++iii) {
		ATA_INFO("try open " << m_apiAvaillable[iii].first);
		if (_api == m_apiAvaillable[iii].first) {
			ATA_INFO("    ==> call it");
			m_rtapi = m_apiAvaillable[iii].second();
			if (m_rtapi != nullptr) {
				return;
			}
		}
	}
	// TODO : An error occured ...
	ATA_ERROR("Error in open API ...");
}


airtaudio::Interface::Interface() :
  m_rtapi(nullptr) {
	ATA_DEBUG("Add interface:");
#if defined(__UNIX_JACK__)
	ATA_DEBUG("    JACK");
	addInterface(airtaudio::type_jack, airtaudio::api::Jack::Create);
#endif
#if defined(__LINUX_ALSA__)
	ATA_DEBUG("    ALSA");
	addInterface(airtaudio::type_alsa, airtaudio::api::Alsa::Create);
#endif
#if defined(__LINUX_PULSE__)
	ATA_DEBUG("    PULSE");
	addInterface(airtaudio::type_pulse, airtaudio::api::Pulse::Create);
#endif
#if defined(__LINUX_OSS__)
	ATA_DEBUG("    OSS");
	addInterface(airtaudio::type_oss, airtaudio::api::Oss::Create);
#endif
#if defined(__WINDOWS_ASIO__)
	ATA_DEBUG("    ASIO");
	addInterface(airtaudio::type_asio, airtaudio::api::Asio::Create);
#endif
#if defined(__WINDOWS_DS__)
	ATA_DEBUG("    DS");
	addInterface(airtaudio::type_ds, airtaudio::api::Ds::Create);
#endif
#if defined(__MACOSX_CORE__)
	ATA_DEBUG("    CORE OSX");
	addInterface(airtaudio::type_coreOSX, airtaudio::api::Core::Create);
#endif
#if defined(__IOS_CORE__)
	ATA_DEBUG("    CORE IOS");
	addInterface(airtaudio::type_coreIOS, airtaudio::api::CoreIos::Create);
#endif
#if defined(__ANDROID_JAVA__)
	ATA_DEBUG("    JAVA");
	addInterface(airtaudio::type_java, airtaudio::api::Android::Create);
#endif
#if defined(__AIRTAUDIO_DUMMY__)
	ATA_DEBUG("    DUMMY");
	addInterface(airtaudio::type_dummy, airtaudio::api::Dummy::Create);
#endif
}

void airtaudio::Interface::addInterface(enum airtaudio::type _api, Api* (*_callbackCreate)()) {
	m_apiAvaillable.push_back(std::pair<enum airtaudio::type, Api* (*)()>(_api, _callbackCreate));
}

enum airtaudio::error airtaudio::Interface::instanciate(enum airtaudio::type _api) {
	ATA_INFO("Instanciate API ...");
	if (m_rtapi != nullptr) {
		ATA_WARNING("Interface already started ...!");
		return airtaudio::error_none;
	}
	if (_api != airtaudio::type_undefined) {
		ATA_INFO("API specified : " << _api);
		// Attempt to open the specified API.
		openRtApi(_api);
		if (m_rtapi != nullptr) {
			if (m_rtapi->getDeviceCount() != 0) {
				ATA_INFO("    ==> api open");
			}
			return airtaudio::error_none;
		}
		// No compiled support for specified API value.	Issue a debug
		// warning and continue as if no API was specified.
		ATA_ERROR("RtAudio: no compiled support for specified API argument!");
		return airtaudio::error_fail;
	}
	ATA_INFO("Auto choice API :");
	// Iterate through the compiled APIs and return as soon as we find
	// one with at least one device or we reach the end of the list.
	std::vector<enum airtaudio::type> apis = getCompiledApi();
	ATA_INFO(" find : " << apis.size() << " apis.");
	for (size_t iii=0; iii<apis.size(); ++iii) {
		ATA_INFO("try open ...");
		openRtApi(apis[iii]);
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
		return airtaudio::error_none;
	}
	ATA_ERROR("RtAudio: no compiled API support found ... critical error!!");
	return airtaudio::error_fail;
}

airtaudio::Interface::~Interface() {
	ATA_INFO("Remove interface");
	delete m_rtapi;
	m_rtapi = nullptr;
}

enum airtaudio::error airtaudio::Interface::openStream(airtaudio::StreamParameters* _outputParameters,
                                                       airtaudio::StreamParameters* _inputParameters,
                                                       audio::format _format,
                                                       uint32_t _sampleRate,
                                                       uint32_t* _bufferFrames,
                                                       airtaudio::AirTAudioCallback _callback,
                                                       const airtaudio::StreamOptions& _options) {
	if (m_rtapi == nullptr) {
		return airtaudio::error_inputNull;
	}
	return m_rtapi->openStream(_outputParameters,
	                           _inputParameters,
	                           _format,
	                           _sampleRate,
	                           _bufferFrames,
	                           _callback,
	                           _options);
}

bool airtaudio::Interface::isMasterOf(airtaudio::Interface& _interface) {
	if (m_rtapi == nullptr) {
		ATA_ERROR("Current Master API is nullptr ...");
		return false;
	}
	if (_interface.m_rtapi == nullptr) {
		ATA_ERROR("Current Slave API is nullptr ...");
		return false;
	}
	if (m_rtapi->getCurrentApi() != _interface.m_rtapi->getCurrentApi()) {
		ATA_ERROR("Can not link 2 Interface with not the same Low level type (???)");//" << _interface.m_adac->getCurrentApi() << " != " << m_adac->getCurrentApi() << ")");
		return false;
	}
	if (m_rtapi->getCurrentApi() != airtaudio::type_alsa) {
		ATA_ERROR("Link 2 device together work only if the interafec is ????");// << airtaudio::type_alsa << " not for " << m_rtapi->getCurrentApi());
		return false;
	}
	return m_rtapi->isMasterOf(_interface.m_rtapi);
}

