/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AIRTAUDIO_API_JACK_H__) && defined(__UNIX_JACK__)
#define __AIRTAUDIO_API_JACK_H__

#include <jack/jack.h>
namespace airtaudio {
	namespace api {
		class JackPrivate;
		class Jack: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Jack();
				virtual ~Jack();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::type_jack;
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
				bool callbackEvent(uint64_t _nframes);
			private:
				static int32_t jackXrun(void* _userData);
				static void jackCloseStream(void* _userData);
				static void jackShutdown(void* _userData);
				static int32_t jackCallbackHandler(jack_nframes_t _nframes, void* _userData);
			private:
				std::unique_ptr<JackPrivate> m_private;
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