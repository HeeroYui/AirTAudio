/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once

#include <etk/types.h>

namespace audio {
	namespace orchestra {
		enum class state {
			closed,
			stopped,
			stopping,
			running
		};
	}
}

