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
				static airtaudio::Api* Create();
			public:
				Dummy();
				airtaudio::api::type getCurrentApi() {
					return airtaudio::api::RTAUDIO_DUMMY;
				}
				uint32_t getDeviceCount();
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::errorType closeStream();
				enum airtaudio::errorType startStream();
				enum airtaudio::errorType stopStream();
				enum airtaudio::errorType abortStream();
			private:
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::api::StreamMode _mode,
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