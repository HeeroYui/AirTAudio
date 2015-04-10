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
		enum type {
			type_undefined, //!< Error API.
			type_alsa, //!< LINUX The Advanced Linux Sound Architecture.
			type_pulse, //!< LINUX The Linux PulseAudio.
			type_oss, //!< LINUX The Linux Open Sound System.
			type_jack, //!< UNIX The Jack Low-Latency Audio Server.
			type_coreOSX, //!< Macintosh OSX Core Audio.
			type_coreIOS, //!< Macintosh iOS Core Audio.
			type_asio, //!< WINDOWS The Steinberg Audio Stream I/O.
			type_ds, //!< WINDOWS The Microsoft Direct Sound.
			type_java, //!< ANDROID Interface.
			type_dummy, //!< Empty wrapper (non-functional).
			type_user1, //!< User interface 1.
			type_user2, //!< User interface 2.
			type_user3, //!< User interface 3.
			type_user4, //!< User interface 4.
		};
		std::ostream& operator <<(std::ostream& _os, const enum audio::orchestra::type& _obj);
		std::ostream& operator <<(std::ostream& _os, const std::vector<enum audio::orchestra::type>& _obj);
		std::string getTypeString(enum audio::orchestra::type _value);
		enum audio::orchestra::type getTypeFromString(const std::string& _value);
	}
}

#endif
