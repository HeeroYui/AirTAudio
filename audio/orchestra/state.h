/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AUDIO_ORCHESTRA_STATE_H__
#define __AUDIO_ORCHESTRA_STATE_H__

#include <etk/types.h>


namespace audio {
	namespace orchestra {
		enum state {
			state_closed,
			state_stopped,
			state_stopping,
			state_running
		};
	}
}

#endif
