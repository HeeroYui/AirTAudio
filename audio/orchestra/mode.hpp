/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once

#include <etk/types.hpp>
#include <etk/Stream.hpp>

namespace audio {
	namespace orchestra {
		enum mode {
			mode_unknow,
			mode_output,
			mode_input,
			mode_duplex
		};
		int32_t modeToIdTable(enum mode _mode);
	}
	etk::Stream& operator <<(etk::Stream& _os, enum audio::orchestra::mode _obj);
}

