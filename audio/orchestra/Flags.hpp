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
