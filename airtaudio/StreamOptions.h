/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_STREAM_OPTION_H__
#define __AIRTAUDIO_STREAM_OPTION_H__

namespace airtaudio {
	class StreamOptions {
		public:
			airtaudio::Flags flags; //!< A bit-mask of stream flags
			uint32_t numberOfBuffers; //!< Number of stream buffers.
			std::string streamName; //!< A stream name (currently used only in Jack).
			// Default constructor.
			StreamOptions() :
			  flags(),
			  numberOfBuffers(0){}
	};
};

#endif

