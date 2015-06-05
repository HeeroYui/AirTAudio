/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AUDIO_ORCHESTRA_API_ALSA_H__) && defined(ORCHESTRA_BUILD_ALSA)
#define __AUDIO_ORCHESTRA_API_ALSA_H__

namespace audio {
	namespace orchestra {
		namespace api {
			class AlsaPrivate;
			class Alsa: public audio::orchestra::Api {
				public:
					static audio::orchestra::Api* create();
				public:
					Alsa();
					virtual ~Alsa();
					enum audio::orchestra::type getCurrentApi() {
						return audio::orchestra::type_alsa;
					}
					uint32_t getDeviceCount();
				private:
					bool getNamedDeviceInfoLocal(const std::string& _deviceName,
					                             audio::orchestra::DeviceInfo& _info,
					                             int32_t _cardId=-1, // Alsa card ID
					                             int32_t _subdevice=-1, // alsa subdevice ID
					                             int32_t _localDeviceId=-1); // local ID of device fined
				public:
					bool getNamedDeviceInfo(const std::string& _deviceName, audio::orchestra::DeviceInfo& _info) {
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
					static void alsaCallbackEventMMap(void* _userData);
				private:
					std11::shared_ptr<AlsaPrivate> m_private;
					std::vector<audio::orchestra::DeviceInfo> m_devices;
					void saveDeviceInfo();
					bool probeDeviceOpen(uint32_t _device,
					                     enum audio::orchestra::mode _mode,
					                     uint32_t _channels,
					                     uint32_t _firstChannel,
					                     uint32_t _sampleRate,
					                     enum audio::format _format,
					                     uint32_t *_bufferSize,
					                     const audio::orchestra::StreamOptions& _options);
					
					virtual bool probeDeviceOpenName(const std::string& _deviceName,
					                                 audio::orchestra::mode _mode,
					                                 uint32_t _channels,
					                                 uint32_t _firstChannel,
					                                 uint32_t _sampleRate,
					                                 audio::format _format,
					                                 uint32_t *_bufferSize,
					                                 const audio::orchestra::StreamOptions& _options);
					virtual audio::Time getStreamTime();
				public:
					bool isMasterOf(audio::orchestra::Api* _api);
			};
		}
	}
}

#endif
