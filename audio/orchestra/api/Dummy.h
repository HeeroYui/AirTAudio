/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AUDIO_ORCHESTRA_DUMMY__) && defined(ORCHESTRA_BUILD_DUMMY)
#define __AUDIO_ORCHESTRA_DUMMY__

#include <audio/orchestra/Interface.h>


namespace audio {
	namespace orchestra {
		namespace api {
			class Dummy: public audio::orchestra::Api {
				public:
					static std::shared_ptr<audio::orchestra::Api> create();
				public:
					Dummy();
					const std::string& getCurrentApi() {
						return audio::orchestra::type_dummy;
					}
					uint32_t getDeviceCount();
					audio::orchestra::DeviceInfo getDeviceInfo(uint32_t _device);
					enum audio::orchestra::error closeStream();
					enum audio::orchestra::error startStream();
					enum audio::orchestra::error stopStream();
					enum audio::orchestra::error abortStream();
				private:
					bool probeDeviceOpen(uint32_t _device,
					                     audio::orchestra::mode _mode,
					                     uint32_t _channels,
					                     uint32_t _firstChannel,
					                     uint32_t _sampleRate,
					                     audio::format _format,
					                     uint32_t *_bufferSize,
					                     const audio::orchestra::StreamOptions& _options);
			};
		}
	}
}

#endif
