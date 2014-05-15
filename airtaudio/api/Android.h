/**
 * @author Edouard DUPIN
 * 
 * @license like MIT (see license file)
 */

#if !defined(__AIRTAUDIO_API_ANDROID_H__) && defined(__ANDROID_JAVA__)
#define __AIRTAUDIO_API_ANDROID_H__

namespace airtaudio {
	namespace api {
		class Android: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Android();
				~Android();
				airtaudio::api::type getCurrentApi() {
					return airtaudio::api::ANDROID_JAVA;
				}
				uint32_t getDeviceCount();
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::errorType closeStream();
				enum airtaudio::errorType startStream();
				enum airtaudio::errorType stopStream();
				enum airtaudio::errorType abortStream();
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				void callbackEvent();
			private:
				std::vector<airtaudio::DeviceInfo> m_devices;
				void saveDeviceInfo();
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::api::StreamMode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     airtaudio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
			private:
				void callBackEvent(void* _data,
				                   int32_t _frameRate);
				static void androidCallBackEvent(void* _data,
				                                 int32_t _frameRate,
				                                 void* _userData);
		};
	};
};

#endif
