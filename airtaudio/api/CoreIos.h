/**
 * @author Edouard DUPIN
 * 
 * @license like MIT (see license file)
 */

#if !defined(__AIRTAUDIO_API_CORE_IOS_H__) && defined(__IOS_CORE__)
#define __AIRTAUDIO_API_CORE_IOS_H__

namespace airtaudio {
	namespace api {
		class CoreIosPrivate;
		class CoreIos: public airtaudio::Api {
			public:
				static airtaudio::Api* Create(void);
			public:
				CoreIos(void);
				~CoreIos(void);
				airtaudio::api::type getCurrentApi(void) {
					return airtaudio::api::IOS_CORE;
				}
				uint32_t getDeviceCount(void);
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::errorType closeStream(void);
				enum airtaudio::errorType startStream(void);
				enum airtaudio::errorType stopStream(void);
				enum airtaudio::errorType abortStream(void);
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				void callbackEvent(void);
			private:
				std::vector<airtaudio::DeviceInfo> m_devices;
				void saveDeviceInfo(void);
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::api::StreamMode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     airtaudio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
			public:
				void callBackEvent(void* _data,
				                   int32_t _frameRate);
			private:
				CoreIosPrivate* m_private;
		};
	};
};

#endif
