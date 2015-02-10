/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_CB_H__
#define __AIRTAUDIO_CB_H__

#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <audio/channel.h>
#include <audio/format.h>
#include <airtaudio/error.h>
#include <airtaudio/status.h>
#include <airtaudio/Flags.h>

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
	
	/**
	 * @brief airtaudio callback function prototype.
	 *
	 * All airtaudio clients must create a function of type AirTAudioCallback
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
	 * @param _nbChunk The number of chunk of input or output
	 *          data in the buffers. The actual buffer size in bytes is
	 *          dependent on the data type and number of channels in use.
	 * 
	 * @param _time The number of seconds that have elapsed since the
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
	                               uint32_t _nbChunk,
	                               const std::chrono::system_clock::time_point& _time,
	                               airtaudio::status _status)> AirTAudioCallback;
}

#include <airtaudio/DeviceInfo.h>
#include <airtaudio/StreamOptions.h>
#include <airtaudio/StreamParameters.h>


#endif


