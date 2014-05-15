/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#ifndef __AIRTAUDIO_STREAM_OPTION_H__
#define __AIRTAUDIO_STREAM_OPTION_H__

namespace airtaudio {
	
	/**
	 * @brief The structure for specifying stream options.
	 * 
	 * The following flags can be OR'ed together to allow a client to
	 * make changes to the default stream behavior:
	 * 
	 * - \e RTAUDIO_NONINTERLEAVED: Use non-interleaved buffers (default = interleaved).
	 * - \e RTAUDIO_MINIMIZE_LATENCY: Attempt to set stream parameters for lowest possible latency.
	 * - \e RTAUDIO_HOG_DEVICE: Attempt grab device for exclusive use.
	 * - \e RTAUDIO_SCHEDULE_REALTIME: Attempt to select realtime scheduling for callback thread.
	 * - \e RTAUDIO_ALSA_USE_DEFAULT: Use the "default" PCM device (ALSA only).
	 * 
	 * By default, RtAudio streams pass and receive audio data from the
	 * client in an interleaved format. By passing the
	 * RTAUDIO_NONINTERLEAVED flag to the openStream() function, audio
	 * data will instead be presented in non-interleaved buffers. In
	 * this case, each buffer argument in the RtAudioCallback function
	 * will point to a single array of data, with \c nFrames samples for
	 * each channel concatenated back-to-back. For example, the first
	 * sample of data for the second channel would be located at index \c
	 * nFrames (assuming the \c buffer pointer was recast to the correct
	 * data type for the stream).
	 * 
	 * Certain audio APIs offer a number of parameters that influence the
	 * I/O latency of a stream. By default, RtAudio will attempt to set
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
	 * The \c priority parameter will only be used if the RTAUDIO_SCHEDULE_REALTIME
	 * flag is set. It defines the thread's realtime priority.
	 * 
	 * If the RTAUDIO_ALSA_USE_DEFAULT flag is set, RtAudio will attempt to
	 * open the "default" PCM device when using the ALSA API. Note that this
	 * will override any specified input or output device id.
	 * 
	 * The \c numberOfBuffers parameter can be used to control stream
	 * latency in the Windows DirectSound, Linux OSS, and Linux Alsa APIs
	 * only. A value of two is usually the smallest allowed. Larger
	 * numbers can potentially result in more robust stream performance,
	 * though likely at the cost of stream latency. The value set by the
	 * user is replaced during execution of the RtAudio::openStream()
	 * function by the value actually used by the system.
	 * 
	 * The \c streamName parameter can be used to set the client name
	 * when using the Jack API. By default, the client name is set to
	 * RtApiJack.	However, if you wish to create multiple instances of
	 * RtAudio with Jack, each instance must have a unique client name.
	 */
	class StreamOptions {
		public:
			airtaudio::streamFlags flags; //!< A bit-mask of stream flags (RTAUDIO_NONINTERLEAVED, RTAUDIO_MINIMIZE_LATENCY, RTAUDIO_HOG_DEVICE, RTAUDIO_ALSA_USE_DEFAULT).
			uint32_t numberOfBuffers; //!< Number of stream buffers.
			std::string streamName; //!< A stream name (currently used only in Jack).
			int32_t priority; //!< Scheduling priority of callback thread (only used with flag RTAUDIO_SCHEDULE_REALTIME).
			// Default constructor.
			StreamOptions() :
			  flags(0),
			  numberOfBuffers(0),
			  priority(0) {}
	};
};

#endif

