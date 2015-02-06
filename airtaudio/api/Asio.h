/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#if !defined(__AIRTAUDIO_API_ASIO_H__) && defined(__WINDOWS_ASIO__)
#define __AIRTAUDIO_API_ASIO_H__

namespace airtaudio {
	namespace api {
		class Asio: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Asio();
				virtual ~Asio();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::WINDOWS_ASIO;
				}
				uint32_t getDeviceCount();
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
				bool callbackEvent(long _bufferIndex);
			private:
				std::vector<airtaudio::DeviceInfo> m_devices;
				void saveDeviceInfo();
				bool m_coInitialized;
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::mode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     audio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
		};
	};
};

#endif
