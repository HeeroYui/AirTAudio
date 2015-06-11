/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

//#include <etk/types.h>
#include <audio/orchestra/Interface.h>
#include <audio/orchestra/debug.h>
#include <iostream>

#undef __class__
#define __class__ "Interface"

std::vector<std::string> audio::orchestra::Interface::getListApi() {
	std::vector<std::string> apis;
	// The order here will control the order of RtAudio's API search in
	// the constructor.
	for (size_t iii=0; iii<m_apiAvaillable.size(); ++iii) {
		apis.push_back(m_apiAvaillable[iii].first);
	}
	return apis;
}



void audio::orchestra::Interface::openApi(const std::string& _api) {
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


audio::orchestra::Interface::Interface() :
  m_rtapi(nullptr) {
	ATA_DEBUG("Add interface:");
#if defined(ORCHESTRA_BUILD_JACK)
	addInterface(audio::orchestra::type_jack, audio::orchestra::api::Jack::create);
#endif
#if defined(ORCHESTRA_BUILD_ALSA)
	addInterface(audio::orchestra::type_alsa, audio::orchestra::api::Alsa::create);
#endif
#if defined(ORCHESTRA_BUILD_PULSE)
	addInterface(audio::orchestra::type_pulse, audio::orchestra::api::Pulse::create);
#endif
#if defined(ORCHESTRA_BUILD_OSS)
	addInterface(audio::orchestra::type_oss, audio::orchestra::api::Oss::create);
#endif
#if defined(ORCHESTRA_BUILD_ASIO)
	addInterface(audio::orchestra::type_asio, audio::orchestra::api::Asio::create);
#endif
#if defined(ORCHESTRA_BUILD_DS)
	addInterface(audio::orchestra::type_ds, audio::orchestra::api::Ds::create);
#endif
#if defined(ORCHESTRA_BUILD_MACOSX_CORE)
	addInterface(audio::orchestra::type_coreOSX, audio::orchestra::api::Core::create);
#endif
#if defined(ORCHESTRA_BUILD_IOS_CORE)
	addInterface(audio::orchestra::type_coreIOS, audio::orchestra::api::CoreIos::create);
#endif
#if defined(ORCHESTRA_BUILD_JAVA)
	addInterface(audio::orchestra::type_java, audio::orchestra::api::Android::create);
#endif
#if defined(ORCHESTRA_BUILD_DUMMY)
	addInterface(audio::orchestra::type_dummy, audio::orchestra::api::Dummy::create);
#endif
}

void audio::orchestra::Interface::addInterface(const std::string& _api, Api* (*_callbackCreate)()) {
	m_apiAvaillable.push_back(std::pair<std::string, Api* (*)()>(_api, _callbackCreate));
}

enum audio::orchestra::error audio::orchestra::Interface::clear() {
	ATA_INFO("Clear API ...");
	if (m_rtapi == nullptr) {
		ATA_WARNING("Interface NOT started!");
		return audio::orchestra::error_none;
	}
	delete m_rtapi;
	m_rtapi = nullptr;
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::Interface::instanciate(const std::string& _api) {
	ATA_INFO("Instanciate API ...");
	if (m_rtapi != nullptr) {
		ATA_WARNING("Interface already started!");
		return audio::orchestra::error_none;
	}
	if (_api != audio::orchestra::type_undefined) {
		ATA_INFO("API specified : " << _api);
		// Attempt to open the specified API.
		openApi(_api);
		if (m_rtapi != nullptr) {
			if (m_rtapi->getDeviceCount() != 0) {
				ATA_INFO("    ==> api open");
			}
			return audio::orchestra::error_none;
		}
		// No compiled support for specified API value.	Issue a debug
		// warning and continue as if no API was specified.
		ATA_ERROR("RtAudio: no compiled support for specified API argument!");
		return audio::orchestra::error_fail;
	}
	ATA_INFO("Auto choice API :");
	// Iterate through the compiled APIs and return as soon as we find
	// one with at least one device or we reach the end of the list.
	std::vector<std::string> apis = getListApi();
	ATA_INFO(" find : " << apis.size() << " apis.");
	for (size_t iii=0; iii<apis.size(); ++iii) {
		ATA_INFO("try open ...");
		openApi(apis[iii]);
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
		return audio::orchestra::error_none;
	}
	ATA_ERROR("RtAudio: no compiled API support found ... critical error!!");
	return audio::orchestra::error_fail;
}

audio::orchestra::Interface::~Interface() {
	ATA_INFO("Remove interface");
	delete m_rtapi;
	m_rtapi = nullptr;
}

enum audio::orchestra::error audio::orchestra::Interface::openStream(audio::orchestra::StreamParameters* _outputParameters,
                                                                     audio::orchestra::StreamParameters* _inputParameters,
                                                                     audio::format _format,
                                                                     uint32_t _sampleRate,
                                                                     uint32_t* _bufferFrames,
                                                                     audio::orchestra::AirTAudioCallback _callback,
                                                                     const audio::orchestra::StreamOptions& _options) {
	if (m_rtapi == nullptr) {
		return audio::orchestra::error_inputNull;
	}
	return m_rtapi->openStream(_outputParameters,
	                           _inputParameters,
	                           _format,
	                           _sampleRate,
	                           _bufferFrames,
	                           _callback,
	                           _options);
}

bool audio::orchestra::Interface::isMasterOf(audio::orchestra::Interface& _interface) {
	if (m_rtapi == nullptr) {
		ATA_ERROR("Current Master API is nullptr ...");
		return false;
	}
	if (_interface.m_rtapi == nullptr) {
		ATA_ERROR("Current Slave API is nullptr ...");
		return false;
	}
	if (m_rtapi->getCurrentApi() != _interface.m_rtapi->getCurrentApi()) {
		ATA_ERROR("Can not link 2 Interface with not the same Low level type (?)");//" << _interface.m_adac->getCurrentApi() << " != " << m_adac->getCurrentApi() << ")");
		return false;
	}
	if (m_rtapi->getCurrentApi() != audio::orchestra::type_alsa) {
		ATA_ERROR("Link 2 device together work only if the interafec is ?");// << audio::orchestra::type_alsa << " not for " << m_rtapi->getCurrentApi());
		return false;
	}
	return m_rtapi->isMasterOf(_interface.m_rtapi);
}

