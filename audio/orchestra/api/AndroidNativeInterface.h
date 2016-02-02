/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 */
#pragma once

#ifdef ORCHESTRA_BUILD_JAVA

#include <audio/orchestra/DeviceInfo.h>
#include <audio/orchestra/mode.h>
#include <audio/orchestra/error.h>
#include <audio/orchestra/StreamOptions.h>
#include <audio/format.h>
#include <memory>

namespace audio {
	namespace orchestra {
		namespace api {
			class Android;
			namespace android {
				uint32_t getDeviceCount();
				audio::orchestra::DeviceInfo getDeviceInfo(uint32_t _device);
				int32_t open(uint32_t _device,
				             audio::orchestra::mode _mode,
				             uint32_t _channels,
				             uint32_t _firstChannel,
				             uint32_t _sampleRate,
				             audio::format _format,
				             uint32_t *_bufferSize,
				             const audio::orchestra::StreamOptions& _options,
				             std11::shared_ptr<audio::orchestra::api::Android> _instance);
				enum audio::orchestra::error closeStream(int32_t _id);
				enum audio::orchestra::error startStream(int32_t _id);
				enum audio::orchestra::error stopStream(int32_t _id);
				enum audio::orchestra::error abortStream(int32_t _id);
			}
		}
	}
}

#endif
