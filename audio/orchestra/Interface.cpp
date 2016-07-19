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
#include <audio/orchestra/api/Alsa.h>
#include <audio/orchestra/api/Android.h>
#include <audio/orchestra/api/Asio.h>
#include <audio/orchestra/api/Core.h>
#include <audio/orchestra/api/CoreIos.h>
#include <audio/orchestra/api/Ds.h>
#include <audio/orchestra/api/Dummy.h>
#include <audio/orchestra/api/Jack.h>
#include <audio/orchestra/api/Pulse.h>

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
	m_api.reset();
	for (size_t iii=0; iii<m_apiAvaillable.size(); ++iii) {
		ATA_INFO("try open " << m_apiAvaillable[iii].first);
		if (_api == m_apiAvaillable[iii].first) {
			ATA_INFO("    ==> call it");
			m_api = m_apiAvaillable[iii].second();
			if (m_api != nullptr) {
				return;
			}
		}
	}
	// TODO : An error occured ...
	ATA_ERROR("Error in open API ...");
}


audio::orchestra::Interface::Interface() :
  m_api(nullptr) {
	ATA_DEBUG("Add interface:");
#if defined(ORCHESTRA_BUILD_JACK)
	addInterface(audio::orchestra::typeJack, audio::orchestra::api::Jack::create);
#endif
#if defined(ORCHESTRA_BUILD_ALSA)
	addInterface(audio::orchestra::typeAlsa, audio::orchestra::api::Alsa::create);
#endif
#if defined(ORCHESTRA_BUILD_PULSE)
	addInterface(audio::orchestra::typePulse, audio::orchestra::api::Pulse::create);
#endif
#if defined(ORCHESTRA_BUILD_ASIO)
	addInterface(audio::orchestra::typeAsio, audio::orchestra::api::Asio::create);
#endif
#if defined(ORCHESTRA_BUILD_DS)
	addInterface(audio::orchestra::typeDs, audio::orchestra::api::Ds::create);
#endif
#if defined(ORCHESTRA_BUILD_MACOSX_CORE)
	addInterface(audio::orchestra::typeCoreOSX, audio::orchestra::api::Core::create);
#endif
#if defined(ORCHESTRA_BUILD_IOS_CORE)
	addInterface(audio::orchestra::typeCoreIOS, audio::orchestra::api::CoreIos::create);
#endif
#if defined(ORCHESTRA_BUILD_JAVA)
	addInterface(audio::orchestra::typeJava, audio::orchestra::api::Android::create);
#endif
#if defined(ORCHESTRA_BUILD_DUMMY)
	addInterface(audio::orchestra::typeDummy, audio::orchestra::api::Dummy::create);
#endif
}

void audio::orchestra::Interface::addInterface(const std::string& _api, ememory::SharedPtr<Api> (*_callbackCreate)()) {
	m_apiAvaillable.push_back(std::pair<std::string, ememory::SharedPtr<Api> (*)()>(_api, _callbackCreate));
}

enum audio::orchestra::error audio::orchestra::Interface::clear() {
	ATA_INFO("Clear API ...");
	if (m_api == nullptr) {
		ATA_WARNING("Interface NOT started!");
		return audio::orchestra::error_none;
	}
	m_api.reset();
	return audio::orchestra::error_none;
}

enum audio::orchestra::error audio::orchestra::Interface::instanciate(const std::string& _api) {
	ATA_INFO("Instanciate API ...");
	if (m_api != nullptr) {
		ATA_WARNING("Interface already started!");
		return audio::orchestra::error_none;
	}
	if (_api != audio::orchestra::typeUndefined) {
		ATA_INFO("API specified : " << _api);
		// Attempt to open the specified API.
		openApi(_api);
		if (m_api != nullptr) {
			if (m_api->getDeviceCount() != 0) {
				ATA_INFO("    ==> api open");
			}
			return audio::orchestra::error_none;
		}
		// No compiled support for specified API value.	Issue a debug
		// warning and continue as if no API was specified.
		ATA_ERROR("API NOT Supported '" << _api << "' not in " << getListApi());
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
		if(m_api == nullptr) {
			ATA_ERROR("    ==> can not create ...");
			continue;
		}
		if (m_api->getDeviceCount() != 0) {
			ATA_INFO("    ==> api open");
			break;
		} else {
			ATA_INFO("    ==> Interface exist, but have no devices: " << m_api->getDeviceCount());
		}
	}
	if (m_api != nullptr) {
		return audio::orchestra::error_none;
	}
	ATA_ERROR("API NOT Supported '" << _api << "' not in " << getListApi());
	return audio::orchestra::error_fail;
}

audio::orchestra::Interface::~Interface() {
	ATA_INFO("Remove interface");
	m_api.reset();
}

enum audio::orchestra::error audio::orchestra::Interface::openStream(audio::orchestra::StreamParameters* _outputParameters,
                                                                     audio::orchestra::StreamParameters* _inputParameters,
                                                                     audio::format _format,
                                                                     uint32_t _sampleRate,
                                                                     uint32_t* _bufferFrames,
                                                                     audio::orchestra::AirTAudioCallback _callback,
                                                                     const audio::orchestra::StreamOptions& _options) {
	if (m_api == nullptr) {
		return audio::orchestra::error_inputNull;
	}
	return m_api->openStream(_outputParameters,
	                           _inputParameters,
	                           _format,
	                           _sampleRate,
	                           _bufferFrames,
	                           _callback,
	                           _options);
}

bool audio::orchestra::Interface::isMasterOf(audio::orchestra::Interface& _interface) {
	if (m_api == nullptr) {
		ATA_ERROR("Current Master API is nullptr ...");
		return false;
	}
	if (_interface.m_api == nullptr) {
		ATA_ERROR("Current Slave API is nullptr ...");
		return false;
	}
	if (m_api->getCurrentApi() != _interface.m_api->getCurrentApi()) {
		ATA_ERROR("Can not link 2 Interface with not the same Low level type (?)");//" << _interface.m_adac->getCurrentApi() << " != " << m_adac->getCurrentApi() << ")");
		return false;
	}
	if (m_api->getCurrentApi() != audio::orchestra::typeAlsa) {
		ATA_ERROR("Link 2 device together work only if the interafec is ?");// << audio::orchestra::type::alsa << " not for " << m_api->getCurrentApi());
		return false;
	}
	return m_api->isMasterOf(_interface.m_api);
}

