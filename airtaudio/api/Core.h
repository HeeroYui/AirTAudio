/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AIRTAUDIO_API_CORE_H__) && defined(__MACOSX_CORE__)
#define __AIRTAUDIO_API_CORE_H__

#include <CoreAudio/AudioHardware.h>

namespace airtaudio {
	namespace api {
		class CorePrivate;
		class Core: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Core();
				virtual ~Core();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::type_coreOSX;
				}
				uint32_t getDeviceCount();
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				uint32_t getDefaultOutputDevice();
				uint32_t getDefaultInputDevice();
				enum airtaudio::error closeStream();
				enum airtaudio::error startStream();
				enum airtaudio::error stopStream();
				enum airtaudio::error abortStream();
				long getStreamLatency();
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				bool callbackEvent(AudioDeviceID _deviceId,
				                   const AudioBufferList *_inBufferList,
				                   const AudioBufferList *_outBufferList);
				static OSStatus callbackEvent(AudioDeviceID _inDevice,
				                              const AudioTimeStamp* _inNow,
				                              const AudioBufferList* _inInputData,
				                              const AudioTimeStamp* _inInputTime,
				                              AudioBufferList* _outOutputData,
				                              const AudioTimeStamp* _inOutputTime,
				                              void* _infoPointer);
				static void coreStopStream(void *_userData);
			private:
				std::unique_ptr<CorePrivate> m_private;
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::mode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     audio::format _format,
				                     uint32_t *_bufferSize,
				                     airtaudio::StreamOptions *_options);
				static const char* getErrorCode(OSStatus _code);
			
			static OSStatus xrunListener(AudioObjectID _inDevice,
			                             uint32_t _nAddresses,
			                             const AudioObjectPropertyAddress _properties[],
			                             void* _userData);
		};
	};
};

#endif
