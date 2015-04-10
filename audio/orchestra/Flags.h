/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AUDIO_ORCHESTRA_FLAGS_H__
#define __AUDIO_ORCHESTRA_FLAGS_H__

#include <etk/types.h>


namespace audio {
	namespace orchestra {
		class Flags {
			public:
				bool m_minimizeLatency; // Simple example ==> TODO ...
				Flags() :
				  m_minimizeLatency(false) {
					// nothing to do ...
				}
		};
	}
}

#endif
