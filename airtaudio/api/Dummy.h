/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#if !defined(__AIRTAUDIO_API_DUMMY_H__) && defined(__AIRTAUDIO_DUMMY__)
#define __AIRTAUDIO_API_DUMMY_H__

#include <airtaudio/Interface.h>

namespace airtaudio {
	namespace api {
		class Dummy: public airtaudio::Api {
			public:
				Dummy(void);
				airtaudio::api::type getCurrentApi(void) {
					return airtaudio::api::RTAUDIO_DUMMY;
				}
				uint32_t getDeviceCount(void);
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				void closeStream(void);
				void startStream(void);
				void stopStream(void);
				void abortStream(void);
			private:
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