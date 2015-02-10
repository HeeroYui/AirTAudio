/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AIRTAUDIO_API_CORE_IOS_H__) && defined(__IOS_CORE__)
#define __AIRTAUDIO_API_CORE_IOS_H__

namespace airtaudio {
	namespace api {
		class CoreIosPrivate;
		class CoreIos: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				CoreIos();
				virtual ~CoreIos();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::type_coreIOS;
				}
				uint32_t getDeviceCount();
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
			private:
				std::vector<airtaudio::DeviceInfo> m_devices;
				void saveDeviceInfo();
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::mode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     audio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
			public:
				void callBackEvent(void* _data,
				                   int32_t _frameRate);
			private:
				std::unique_ptr<CoreIosPrivate> m_private;
				static OSStatus playbackCallback(void *_userData,
				                                 AudioUnitRenderActionFlags* _ioActionFlags,
				                                 const AudioTimeStamp* _inTimeStamp,
				                                 uint32_t _inBusNumber,
				                                 uint32_t _inNumberFrames,
				                                 AudioBufferList* _ioData);
		};
	};
};

#endif
