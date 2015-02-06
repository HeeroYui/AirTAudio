/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#ifndef __AIRTAUDIO_ERROR_H__
#define __AIRTAUDIO_ERROR_H__

#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <audio/channel.h>
#include <audio/format.h>

// defien type : uintXX_t and intXX_t
#define __STDC_LIMIT_MACROS
// note in android include the macro of min max are overwitten
#include <stdint.h>

#if defined(HAVE_GETTIMEOFDAY)
	#include <sys/time.h>
#endif
//#include <etk/Stream.h>

namespace airtaudio {
	//! Defined error types.
	enum error {
		error_none, //!< No error
		error_fail, //!< An error occure in the operation
		error_warning, //!< A non-critical error.
		error_inputNull, //!< null input or internal errror
		error_invalidUse, //!< The function was called incorrectly.
		error_systemError //!< A system error occured.
	};
	
	class Flags {
		public:
			bool m_minimizeLatency; // Simple example ==> TODO ...
			Flags() :
			  m_minimizeLatency(false) {
				// nothing to do ...
			}
	};
	enum status {
		status_ok, //!< nothing...
		status_overflow, //!< Internal buffer has more data than they can accept
		status_underflow //!< The internal buffer is empty
	};
	/**
	 * @brief RtAudio callback function prototype.
	 *
	 * All RtAudio clients must create a function of type RtAudioCallback
	 * to read and/or write data from/to the audio stream. When the
	 * underlying audio system is ready for new input or output data, this
	 * function will be invoked.
	 *
	 * @param _outputBuffer For output (or duplex) streams, the client
	 *          should write \c nFrames of audio sample frames into this
	 *          buffer.	This argument should be recast to the datatype
	 *          specified when the stream was opened. For input-only
	 *          streams, this argument will be nullptr.
	 * 
	 * @param _inputBuffer For input (or duplex) streams, this buffer will
	 *          hold \c nFrames of input audio sample frames. This
	 *          argument should be recast to the datatype specified when the
	 *          stream was opened.	For output-only streams, this argument
	 *          will be nullptr.
	 * 
	 * @param _nFrames The number of sample frames of input or output
	 *          data in the buffers. The actual buffer size in bytes is
	 *          dependent on the data type and number of channels in use.
	 * 
	 * @param _streamTime The number of seconds that have elapsed since the
	 *          stream was started.
	 * 
	 * @param _status If non-zero, this argument indicates a data overflow
	 *          or underflow condition for the stream. The particular
	 *          condition can be determined by comparison with the
	 *          streamStatus flags.
	 * 
	 * To continue normal stream operation, the RtAudioCallback function
	 * should return a value of zero. To stop the stream and drain the
	 * output buffer, the function should return a value of one. To abort
	 * the stream immediately, the client should return a value of two.
	 */
	typedef std::function<int32_t (void* _outputBuffer,
	                               void* _inputBuffer,
	                               uint32_t _nFrames,
	                               double _streamTime,
	                               airtaudio::status _status)> AirTAudioCallback;
}

#include <airtaudio/DeviceInfo.h>
#include <airtaudio/StreamOptions.h>
#include <airtaudio/StreamParameters.h>


#endif


