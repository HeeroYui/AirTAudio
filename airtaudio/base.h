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

// defien type : uintXX_t and intXX_t
#define __STDC_LIMIT_MACROS
// note in android include the macro of min max are overwitten
#include <stdint.h>

#if defined(HAVE_GETTIMEOFDAY)
	#include <sys/time.h>
#endif
//#include <etk/Stream.h>

namespace airtaudio {
	//! Defined RtError types.
	enum errorType {
		errorNone, //!< No error
		errorFail, //!< An error occure in the operation
		errorWarning, //!< A non-critical error.
		errorInputNull, //!< null input or internal errror
		errorInvalidUse, //!< The function was called incorrectly.
		errorSystemError //!< A system error occured.
	};
	// airtaudio version
	static const std::string VERSION("4.0.12");
	
	/**
	 * @brief Debug operator To display the curent element in a Human redeable information
	 */
	//std::ostream& operator <<(std::ostream& _os, enum errorType _obj);
	/**
	 * @typedef typedef uint64_t format;
	 * @brief airtaudio data format type.
	 *
	 * Support for signed integers and floats.	Audio data fed to/from an
	 * airtaudio stream is assumed to ALWAYS be in host byte order.	The
	 * internal routines will automatically take care of any necessary
	 * byte-swapping between the host format and the soundcard.	Thus,
	 * endian-ness is not a concern in the following format definitions.
	 *
	 * - \e SINT8:	 8-bit signed integer.
	 * - \e SINT16:	16-bit signed integer.
	 * - \e SINT24:	24-bit signed integer.
	 * - \e SINT32:	32-bit signed integer.
	 * - \e FLOAT32: Normalized between plus/minus 1.0.
	 * - \e FLOAT64: Normalized between plus/minus 1.0.
	 */
	typedef uint64_t format;
	static const format SINT8 = 0x1;		// 8-bit signed integer.
	static const format SINT16 = 0x2;	 // 16-bit signed integer.
	static const format SINT24 = 0x4;	 // 24-bit signed integer.
	static const format SINT32 = 0x8;	 // 32-bit signed integer.
	static const format FLOAT32 = 0x10; // Normalized between plus/minus 1.0.
	static const format FLOAT64 = 0x20; // Normalized between plus/minus 1.0.
	
	/**
	 * @brief Debug operator To display the curent element in a Human redeable information
	 */
	//std::ostream& operator <<(std::ostream& _os, const airtaudio::format& _obj);
	
	/**
	 * @typedef typedef uint64_t streamFlags;
	 * @brief RtAudio stream option flags.
	 * 
	 * The following flags can be OR'ed together to allow a client to
	 * make changes to the default stream behavior:
	 * 
	 * - \e NONINTERLEAVED:	 Use non-interleaved buffers (default = interleaved).
	 * - \e MINIMIZE_LATENCY: Attempt to set stream parameters for lowest possible latency.
	 * - \e HOG_DEVICE:			 Attempt grab device for exclusive use.
	 * - \e ALSA_USE_DEFAULT: Use the "default" PCM device (ALSA only).
	 * 
	 * By default, RtAudio streams pass and receive audio data from the
	 * client in an interleaved format.	By passing the
	 * RTAUDIO_NONINTERLEAVED flag to the openStream() function, audio
	 * data will instead be presented in non-interleaved buffers.	In
	 * this case, each buffer argument in the RtAudioCallback function
	 * will point to a single array of data, with \c nFrames samples for
	 * each channel concatenated back-to-back.	For example, the first
	 * sample of data for the second channel would be located at index \c
	 * nFrames (assuming the \c buffer pointer was recast to the correct
	 * data type for the stream).
	 * 
	 * Certain audio APIs offer a number of parameters that influence the
	 * I/O latency of a stream.	By default, RtAudio will attempt to set
	 * these parameters internally for robust (glitch-free) performance
	 * (though some APIs, like Windows Direct Sound, make this difficult).
	 * By passing the RTAUDIO_MINIMIZE_LATENCY flag to the openStream()
	 * function, internal stream settings will be influenced in an attempt
	 * to minimize stream latency, though possibly at the expense of stream
	 * performance.
	 * 
	 * If the RTAUDIO_HOG_DEVICE flag is set, RtAudio will attempt to
	 * open the input and/or output stream device(s) for exclusive use.
	 * Note that this is not possible with all supported audio APIs.
	 * 
	 * If the RTAUDIO_SCHEDULE_REALTIME flag is set, RtAudio will attempt 
	 * to select realtime scheduling (round-robin) for the callback thread.
	 * 
	 * If the RTAUDIO_ALSA_USE_DEFAULT flag is set, RtAudio will attempt to
	 * open the "default" PCM device when using the ALSA API. Note that this
	 * will override any specified input or output device id.
	*/
	typedef uint32_t streamFlags;
	static const streamFlags NONINTERLEAVED = 0x1; // Use non-interleaved buffers (default = interleaved).
	static const streamFlags MINIMIZE_LATENCY = 0x2; // Attempt to set stream parameters for lowest possible latency.
	static const streamFlags HOG_DEVICE = 0x4; // Attempt grab device and prevent use by others.
	static const streamFlags SCHEDULE_REALTIME = 0x8; // Try to select realtime scheduling for callback thread.
	static const streamFlags ALSA_USE_DEFAULT = 0x10; // Use the "default" PCM device (ALSA only).
	
	/**
	 * @brief Debug operator To display the curent element in a Human redeable information
	 */
	//std::ostream& operator <<(std::ostream& _os, const airtaudio::streamFlags& _obj);
	
	/**
	 * @typedef typedef uint64_t rtaudio::streamStatus;
	 * @brief RtAudio stream status (over- or underflow) flags.
	 * 
	 * Notification of a stream over- or underflow is indicated by a
	 * non-zero stream \c status argument in the RtAudioCallback function.
	 * The stream status can be one of the following two options,
	 * depending on whether the stream is open for output and/or input:
	 * 
	 * - \e RTAUDIO_INPUT_OVERFLOW: Input data was discarded because of an overflow condition at the driver.
	 * - \e RTAUDIO_OUTPUT_UNDERFLOW: The output buffer ran low, likely producing a break in the output sound.
	 */
	typedef uint32_t streamStatus;
	static const streamStatus INPUT_OVERFLOW = 0x1; // Input data was discarded because of an overflow condition at the driver.
	static const streamStatus OUTPUT_UNDERFLOW = 0x2; // The output buffer ran low, likely causing a gap in the output sound.
	
	/**
	 * @brief Debug operator To display the curent element in a Human redeable information
	 */
	//std::ostream& operator <<(std::ostream& _os, const airtaudio::streamStatus& _obj);
	
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
	                               airtaudio::streamStatus _status)> AirTAudioCallback;
}

#include <airtaudio/DeviceInfo.h>
#include <airtaudio/StreamOptions.h>
#include <airtaudio/StreamParameters.h>


#endif


