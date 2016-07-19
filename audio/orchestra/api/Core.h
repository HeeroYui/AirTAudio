/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once
#ifdef ORCHESTRA_BUILD_MACOSX_CORE

#include <CoreAudio/AudioHardware.h>


namespace audio {
	namespace orchestra {
		namespace api {
			class CorePrivate;
			class Core: public audio::orchestra::Api {
				public:
					static ememory::SharedPtr<audio::orchestra::Api> create();
				public:
					Core();
					virtual ~Core();
					const std::string& getCurrentApi() {
						return audio::orchestra::typeCoreOSX;
					}
					uint32_t getDeviceCount();
					audio::orchestra::DeviceInfo getDeviceInfo(uint32_t _device);
					uint32_t getDefaultOutputDevice();
					uint32_t getDefaultInputDevice();
					enum audio::orchestra::error closeStream();
					enum audio::orchestra::error startStream();
					enum audio::orchestra::error stopStream();
					enum audio::orchestra::error abortStream();
					long getStreamLatency();
					bool callbackEvent(AudioDeviceID _deviceId,
					                   const AudioBufferList *_inBufferList,
					                   const audio::Time& _inTime,
					                   const AudioBufferList *_outBufferList,
					                   const audio::Time& _outTime);
					static OSStatus callbackEvent(AudioDeviceID _inDevice,
					                              const AudioTimeStamp* _inNow,
					                              const AudioBufferList* _inInputData,
					                              const AudioTimeStamp* _inInputTime,
					                              AudioBufferList* _outOutputData,
					                              const AudioTimeStamp* _inOutputTime,
					                              void* _infoPointer);
					static void coreStopStream(void *_userData);
				private:
					ememory::SharedPtr<CorePrivate> m_private;
					bool open(uint32_t _device,
					          audio::orchestra::mode _mode,
					          uint32_t _channels,
					          uint32_t _firstChannel,
					          uint32_t _sampleRate,
					          audio::format _format,
					          uint32_t *_bufferSize,
					          const audio::orchestra::StreamOptions& _options);
					static const char* getErrorCode(OSStatus _code);
					static OSStatus xrunListener(AudioObjectID _inDevice,
					                             uint32_t _nAddresses,
					                             const AudioObjectPropertyAddress _properties[],
					                             void* _userData);
			};
		}
	}
}

#endif
