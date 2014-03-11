/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#if !defined(__AIRTAUDIO_API_CORE_H__) && defined(__MACOSX_CORE__)
#define __AIRTAUDIO_API_CORE_H__

#include <CoreAudio/AudioHardware.h>

namespace airtaudio {
	namespace api {
		class Core: public airtaudio::Api {
			public:
				static airtaudio::Api* Create(void);
			public:
				Core();
				~Core();
				airtaudio::api::type getCurrentApi(void) {
					return airtaudio::api::MACOSX_CORE;
				}
				uint32_t getDeviceCount(void);
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				uint32_t getDefaultOutputDevice(void);
				uint32_t getDefaultInputDevice(void);
				void closeStream(void);
				void startStream(void);
				void stopStream(void);
				void abortStream(void);
				long getStreamLatency(void);
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				bool callbackEvent(AudioDeviceID _deviceId,
				                   const AudioBufferList *_inBufferList,
				                   const AudioBufferList *_outBufferList);
				
			private:
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::api::StreamMode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     airtaudio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
				static const char* getErrorCode(OSStatus _code);
		};
	};
};

#endif
