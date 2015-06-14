/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AUDIO_ORCHESTRA_API_PULSE_DEVICE_H__) && defined(ORCHESTRA_BUILD_PULSE)
#define __AUDIO_ORCHESTRA_API_PULSE_DEVICE_H__

#include <etk/types.h>
#include <audio/orchestra/DeviceInfo.h>

namespace audio {
	namespace orchestra {
		namespace api {
			namespace pulse {
				std::vector<audio::orchestra::DeviceInfo> getDeviceList();
			}
		}
	}
}

#endif