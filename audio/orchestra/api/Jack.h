/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AUDIO_ORCHESTRA_API_JACK_H__) && defined(ORCHESTRA_BUILD_JACK)
#define __AUDIO_ORCHESTRA_API_JACK_H__

#include <jack/jack.h>

namespace audio {
	namespace orchestra {
		namespace api {
			class JackPrivate;
			class Jack: public audio::orchestra::Api {
				public:
					static audio::orchestra::Api* create();
				public:
					Jack();
					virtual ~Jack();
					enum audio::orchestra::type getCurrentApi() {
						return audio::orchestra::type_jack;
					}
					uint32_t getDeviceCount();
					audio::orchestra::DeviceInfo getDeviceInfo(uint32_t _device);
					enum audio::orchestra::error closeStream();
					enum audio::orchestra::error startStream();
					enum audio::orchestra::error stopStream();
					enum audio::orchestra::error abortStream();
					long getStreamLatency();
					// This function is intended for internal use only.	It must be
					// public because it is called by the internal callback handler,
					// which is not a member of RtAudio.	External use of this function
					// will most likely produce highly undesireable results!
					bool callbackEvent(uint64_t _nframes);
				private:
					static int32_t jackXrun(void* _userData);
					static void jackCloseStream(void* _userData);
					static void jackShutdown(void* _userData);
					static int32_t jackCallbackHandler(jack_nframes_t _nframes, void* _userData);
				private:
					std11::shared_ptr<JackPrivate> m_private;
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