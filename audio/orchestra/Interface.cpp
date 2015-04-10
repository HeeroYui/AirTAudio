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

std::vector<enum audio::orchestra::type> audio::orchestra::Interface::getCompiledApi() {
	std::vector<enum audio::orchestra::type> apis;
	// The order here will control the order of RtAudio's API search in
	// the constructor.
	for (size_t iii=0; iii<m_apiAvaillable.size(); ++iii) {
		apis.push_back(m_apiAvaillable[iii].first);
	}
	return apis;
}



void audio::orchestra::Interface::openRtApi(enum audio::orchestra::type _api) {
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
	ATA_DEBUG("    JACK");
	addInterface(audio::orchestra::type_jack, audio::orchestra::api::Jack::Create);
#endif
#if defined(ORCHESTRA_BUILD_ALSA)
	ATA_DEBUG("    ALSA");
	addInterface(audio::orchestra::type_alsa, audio::orchestra::api::Alsa::Create);
#endif
#if defined(ORCHESTRA_BUILD_PULSE)
	ATA_DEBUG("    PULSE");
	addInterface(audio::orchestra::type_pulse, audio::orchestra::api::Pulse::Create);
#endif
#if defined(ORCHESTRA_BUILD_OSS)
	ATA_DEBUG("    OSS");
	addInterface(audio::orchestra::type_oss, audio::orchestra::api::Oss::Create);
#endif
#if defined(ORCHESTRA_BUILD_ASIO)
	ATA_DEBUG("    ASIO");
	addInterface(audio::orchestra::type_asio, audio::orchestra::api::Asio::Create);
#endif
#if defined(ORCHESTRA_BUILD_DS)
	ATA_DEBUG("    DS");
	addInterface(audio::orchestra::type_ds, audio::orchestra::api::Ds::Create);
#endif
#if defined(ORCHESTRA_BUILD_MACOSX_CORE)
	ATA_DEBUG("    CORE OSX");
	addInterface(audio::orchestra::type_coreOSX, audio::orchestra::api::Core::Create);
#endif
#if defined(ORCHESTRA_BUILD_IOS_CORE)
	ATA_DEBUG("    CORE IOS");
	addInterface(audio::orchestra::type_coreIOS, audio::orchestra::api::CoreIos::Create);
#endif
#if defined(ORCHESTRA_BUILD_JAVA)
	ATA_DEBUG("    JAVA");
	addInterface(audio::orchestra::type_java, audio::orchestra::api::Android::Create);
#endif
#if defined(ORCHESTRA_BUILD_DUMMY)
	ATA_DEBUG("    DUMMY");
	addInterface(audio::orchestra::type_dummy, audio::orchestra::api::Dummy::Create);
#endif
}

void audio::orchestra::Interface::addInterface(enum audio::orchestra::type _api, Api* (*_callbackCreate)()) {
	m_apiAvaillable.push_back(std::pair<enum audio::orchestra::type, Api* (*)()>(_api, _callbackCreate));
}

enum audio::orchestra::error audio::orchestra::Interface::instanciate(enum audio::orchestra::type _api) {
	ATA_INFO("Instanciate API ...");
	if (m_rtapi != nullptr) {
		ATA_WARNING("Interface already started ...!");
		return audio::orchestra::error_none;
	}
	if (_api != audio::orchestra::type_undefined) {
		ATA_INFO("API specified : " << _api);
		// Attempt to open the specified API.
		openRtApi(_api);
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
	std::vector<enum audio::orchestra::type> apis = getCompiledApi();
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

