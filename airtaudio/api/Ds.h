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
				Ds(void);
				~Ds(void);
				airtaudio::api::type getCurrentApi(void) {
					return airtaudio::api::WINDOWS_DS;
				}
				uint32_t getDeviceCount(void);
				uint32_t getDefaultOutputDevice(void);
				uint32_t getDefaultInputDevice(void);
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				void closeStream(void);
				void startStream(void);
				void stopStream(void);
				void abortStream(void);
				long getStreamLatency(void);
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				void callbackEvent(void);
			private:
				bool m_coInitialized;
				bool m_buffersRolling;
				long m_duplexPrerollBytes;
				std::vector<struct DsDevice> dsDevices;
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::api::StreamMode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     airtaudio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
		};
	};
};

#endif
