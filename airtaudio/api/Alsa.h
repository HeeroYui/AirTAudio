/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AIRTAUDIO_API_ALSA_H__) && defined(__LINUX_ALSA__)
#define __AIRTAUDIO_API_ALSA_H__

namespace airtaudio {
	namespace api {
		class AlsaPrivate;
		class Alsa: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Alsa();
				virtual ~Alsa();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::type_alsa;
				}
				uint32_t getDeviceCount();
			private:
				bool getNamedDeviceInfoLocal(const std::string& _deviceName,
				                             airtaudio::DeviceInfo& _info,
				                             int32_t _cardId=-1, // Alsa card ID
				                             int32_t _subdevice=-1, // alsa subdevice ID
				                             int32_t _localDeviceId=-1); // local ID of device fined
			public:
				bool getNamedDeviceInfo(const std::string& _deviceName, airtaudio::DeviceInfo& _info) {
					return getNamedDeviceInfoLocal(_deviceName, _info);
				}
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::error closeStream();
				enum airtaudio::error startStream();
				enum airtaudio::error stopStream();
				enum airtaudio::error abortStream();
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				void callbackEvent();
				void callbackEventOneCycle();
			private:
				static void alsaCallbackEvent(void* _userData);
			private:
				std11::shared_ptr<AlsaPrivate> m_private;
				std::vector<airtaudio::DeviceInfo> m_devices;
				void saveDeviceInfo();
				bool probeDeviceOpen(uint32_t _device,
				                     enum airtaudio::mode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     enum audio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
				
				virtual bool probeDeviceOpenName(const std::string& _deviceName,
				                                 airtaudio::mode _mode,
				                                 uint32_t _channels,
				                                 uint32_t _firstChannel,
				                                 uint32_t _sampleRate,
				                                 audio::format _format,
				                                 uint32_t *_bufferSize,
				                                 airtaudio::StreamOptions *_options);
				virtual std11::chrono::system_clock::time_point getStreamTime();
		};
	};
};

#endif
