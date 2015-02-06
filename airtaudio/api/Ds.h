/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#if !defined(__AIRTAUDIO_API_DS_H__) && defined(__WINDOWS_DS__)
#define __AIRTAUDIO_API_DS_H__

namespace airtaudio {
	namespace api {
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
				bool m_coInitialized;
				bool m_buffersRolling;
				long m_duplexPrerollBytes;
				std::vector<struct DsDevice> dsDevices;
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
