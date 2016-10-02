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
		extern const std::string typeUndefined; //!< Error API.
		extern const std::string typeAlsa; //!< LINUX The Advanced Linux Sound Architecture.
		extern const std::string typePulse; //!< LINUX The Linux PulseAudio.
		extern const std::string typeOss; //!< LINUX The Linux Open Sound System.
		extern const std::string typeJack; //!< UNIX The Jack Low-Latency Audio Server.
		extern const std::string typeCoreOSX; //!< Macintosh OSX Core Audio.
		extern const std::string typeCoreIOS; //!< Macintosh iOS Core Audio.
		extern const std::string typeAsio; //!< WINDOWS The Steinberg Audio Stream I/O.
		extern const std::string typeDs; //!< WINDOWS The Microsoft Direct Sound.
		extern const std::string typeJava; //!< ANDROID Interface.
		extern const std::string typeDummy; //!< Empty wrapper (non-functional).
	}
}

