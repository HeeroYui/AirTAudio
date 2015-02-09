/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_STATE_H__
#define __AIRTAUDIO_STATE_H__

#include <etk/types.h>

namespace airtaudio {
	enum state {
		state_closed,
		state_stopped,
		state_stopping,
		state_running
	};
}

#endif
