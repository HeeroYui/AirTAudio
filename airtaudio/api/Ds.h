/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AIRTAUDIO_API_DS_H__) && defined(__WINDOWS_DS__)
#define __AIRTAUDIO_API_DS_H__

namespace airtaudio {
	namespace api {
		class DsPrivate;
		class Ds: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Ds();
				virtual ~Ds();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::type_ds;
				}
				uint32_t getDeviceCount();
				uint32_t getDefaultOutputDevice();
				uint32_t getDefaultInputDevice();
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::error closeStream();
				enum airtaudio::error startStream();
				enum airtaudio::error stopStream();
				enum airtaudio::error abortStream();
				long getStreamLatency();
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				void callbackEvent();
			private:
				static void dsCallbackEvent(void *_userData);
				std11::shared_ptr<DsPrivate> m_private;
				bool m_coInitialized;
				bool m_buffersRolling;
				long m_duplexPrerollBytes;
				bool probeDeviceOpen(uint32_t _device,
				                     enum airtaudio::mode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     enum audio::format _format,
				                     uint32_t *_bufferSize,
				                     const airtaudio.::StreamOptions& _options);
		};
	};
};

#endif
