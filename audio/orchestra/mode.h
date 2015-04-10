/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AUDIO_ORCHESTRA_MODE_H__
#define __AUDIO_ORCHESTRA_MODE_H__

#include <etk/types.h>


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
}

#endif
