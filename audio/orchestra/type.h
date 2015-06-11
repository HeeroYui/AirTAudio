/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AUDIO_ORCHESTRA_TYPE_H__
#define __AUDIO_ORCHESTRA_TYPE_H__

#include <etk/types.h>
#include <etk/stdTools.h>


namespace audio {
	namespace orchestra {
		/**
		 * @brief Audio API specifier arguments.
		 */
		extern const std::string type_undefined; //!< Error API.
		extern const std::string type_alsa; //!< LINUX The Advanced Linux Sound Architecture.
		extern const std::string type_pulse; //!< LINUX The Linux PulseAudio.
		extern const std::string type_oss; //!< LINUX The Linux Open Sound System.
		extern const std::string type_jack; //!< UNIX The Jack Low-Latency Audio Server.
		extern const std::string type_coreOSX; //!< Macintosh OSX Core Audio.
		extern const std::string type_coreIOS; //!< Macintosh iOS Core Audio.
		extern const std::string type_asio; //!< WINDOWS The Steinberg Audio Stream I/O.
		extern const std::string type_ds; //!< WINDOWS The Microsoft Direct Sound.
		extern const std::string type_java; //!< ANDROID Interface.
		extern const std::string type_dummy; //!< Empty wrapper (non-functional).
	}
}

#endif
