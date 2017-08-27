/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once

#include <etk/types.hpp>
#include <etk/stdTools.hpp>

namespace audio {
	namespace orchestra {
		/**
		 * @brief Audio API specifier arguments.
		 */
		extern const etk::String typeUndefined; //!< Error API.
		extern const etk::String typeAlsa; //!< LINUX The Advanced Linux Sound Architecture.
		extern const etk::String typePulse; //!< LINUX The Linux PulseAudio.
		extern const etk::String typeOss; //!< LINUX The Linux Open Sound System.
		extern const etk::String typeJack; //!< UNIX The Jack Low-Latency Audio Server.
		extern const etk::String typeCoreOSX; //!< Macintosh OSX Core Audio.
		extern const etk::String typeCoreIOS; //!< Macintosh iOS Core Audio.
		extern const etk::String typeAsio; //!< WINDOWS The Steinberg Audio Stream I/O.
		extern const etk::String typeDs; //!< WINDOWS The Microsoft Direct Sound.
		extern const etk::String typeJava; //!< ANDROID Interface.
		extern const etk::String typeDummy; //!< Empty wrapper (non-functional).
	}
}

