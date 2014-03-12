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
				static airtaudio::Api* Create(void);
			public:
				Dummy(void);
				airtaudio::api::type getCurrentApi(void) {
					return airtaudio::api::RTAUDIO_DUMMY;
				}
				uint32_t getDeviceCount(void);
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::errorType closeStream(void);
				enum airtaudio::errorType startStream(void);
				enum airtaudio::errorType stopStream(void);
				enum airtaudio::errorType abortStream(void);
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