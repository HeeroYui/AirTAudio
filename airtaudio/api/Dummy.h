/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#if !defined(__AIRTAUDIO_DUMMY__) && defined(__DUMMY__)
#define __AIRTAUDIO_DUMMY__

#include <airtaudio/Interface.h>

namespace airtaudio {
	namespace api {
		class Dummy: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Dummy();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::type_dummy;
				}
				uint32_t getDeviceCount();
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::error closeStream();
				enum airtaudio::error startStream();
				enum airtaudio::error stopStream();
				enum airtaudio::error abortStream();
			private:
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