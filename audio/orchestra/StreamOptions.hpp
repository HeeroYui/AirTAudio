/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once

#include <audio/orchestra/Flags.hpp>

namespace audio {
	namespace orchestra {
		enum timestampMode {
			timestampMode_Hardware, //!< enable harware timestamp
			timestampMode_trigered, //!< get harware triger time stamp and increment with duration
			timestampMode_soft, //!< Simulate all timestamp.
		};
		std::ostream& operator <<(std::ostream& _os, enum audio::orchestra::timestampMode _obj);
		
		class StreamOptions {
			public:
				audio::orchestra::Flags flags; //!< A bit-mask of stream flags
				uint32_t numberOfBuffers; //!< Number of stream buffers.
				std::string streamName; //!< A stream name (currently used only in Jack).
				enum timestampMode mode; //!< mode of timestamping data...
				// Default constructor.
				StreamOptions() :
				  flags(),
				  numberOfBuffers(0),
				  mode(timestampMode_Hardware) {}
		};
	}
}

