/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once

#include <etk/types.hpp>

namespace audio {
	namespace orchestra {
		enum class status {
			ok, //!< nothing...
			overflow, //!< Internal buffer has more data than they can accept
			underflow //!< The internal buffer is empty
		};
		etk::Stream& operator <<(etk::Stream& _os, enum audio::orchestra::status _obj);
		etk::Stream& operator <<(etk::Stream& _os, const etk::Vector<enum audio::orchestra::status>& _obj);
	}
}

