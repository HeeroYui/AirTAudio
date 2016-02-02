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
		enum status {
			status_ok, //!< nothing...
			status_overflow, //!< Internal buffer has more data than they can accept
			status_underflow //!< The internal buffer is empty
		};
		std::ostream& operator <<(std::ostream& _os, enum audio::orchestra::status _obj);
		std::ostream& operator <<(std::ostream& _os, const std::vector<enum audio::orchestra::status>& _obj);
	}
}

