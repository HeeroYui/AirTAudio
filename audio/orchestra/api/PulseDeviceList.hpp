/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once
#ifdef ORCHESTRA_BUILD_PULSE

#include <etk/types.hpp>
#include <audio/orchestra/DeviceInfo.hpp>

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