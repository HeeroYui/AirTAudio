/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once

#ifdef ORCHESTRA_BUILD_ALSA

namespace audio {
	namespace orchestra {
		namespace api {
			class AlsaPrivate;
			class Alsa: public audio::orchestra::Api {
				public:
					static ememory::SharedPtr<audio::orchestra::Api> create();
				public:
					Alsa();
					virtual ~Alsa();
					const etk::String& getCurrentApi() {
						return audio::orchestra::typeAlsa;
					}
					uint32_t getDeviceCount();
				private:
					bool getNamedDeviceInfoLocal(const etk::String& _deviceName,
					                             audio::orchestra::DeviceInfo& _info,
					                             int32_t _cardId=-1, // Alsa card ID
					                             int32_t _subdevice=-1, // alsa subdevice ID
					                             int32_t _localDeviceId=-1,// local ID of device find
					                             bool _input=false);
				public:
					bool getNamedDeviceInfo(const etk::String& _deviceName, audio::orchestra::DeviceInfo& _info) {
						return getNamedDeviceInfoLocal(_deviceName, _info);
					}
					audio::orchestra::DeviceInfo getDeviceInfo(uint32_t _device);
					enum audio::orchestra::error closeStream();
					enum audio::orchestra::error startStream();
					enum audio::orchestra::error stopStream();
					enum audio::orchestra::error abortStream();
					// This function is intended for internal use only.	It must be
					// public because it is called by the internal callback handler,
					// which is not a member of RtAudio.	External use of this function
					// will most likely produce highly undesireable results!
					void callbackEvent();
					void callbackEventOneCycleRead();
					void callbackEventOneCycleWrite();
					void callbackEventOneCycleMMAPRead();
					void callbackEventOneCycleMMAPWrite();
				private:
					static void alsaCallbackEvent(void* _userData);
				private:
					ememory::SharedPtr<AlsaPrivate> m_private;
					etk::Vector<audio::orchestra::DeviceInfo> m_devices;
					void saveDeviceInfo();
					bool open(uint32_t _device,
					          enum audio::orchestra::mode _mode,
					          uint32_t _channels,
					          uint32_t _firstChannel,
					          uint32_t _sampleRate,
					          enum audio::format _format,
					          uint32_t *_bufferSize,
					          const audio::orchestra::StreamOptions& _options);
					
					bool openName(const etk::String& _deviceName,
					              audio::orchestra::mode _mode,
					              uint32_t _channels,
					              uint32_t _firstChannel,
					              uint32_t _sampleRate,
					              audio::format _format,
					              uint32_t *_bufferSize,
					              const audio::orchestra::StreamOptions& _options);
					virtual audio::Time getStreamTime();
				public:
					bool isMasterOf(ememory::SharedPtr<audio::orchestra::Api> _api);
			};
		}
	}
}

#endif
