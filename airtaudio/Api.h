/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#ifndef __AIRTAUDIO_API_H__
#define __AIRTAUDIO_API_H__

#include <sstream>

namespace airtaudio {
	namespace api {
		/**
		 * @brief Audio API specifier arguments.
		 */
		enum type {
			UNSPECIFIED, //!< Search for a working compiled API.
			LINUX_ALSA, //!< The Advanced Linux Sound Architecture API.
			LINUX_PULSE, //!< The Linux PulseAudio API.
			LINUX_OSS, //!< The Linux Open Sound System API.
			UNIX_JACK, //!< The Jack Low-Latency Audio Server API.
			MACOSX_CORE, //!< Macintosh OS-X Core Audio API.
			WINDOWS_ASIO, //!< The Steinberg Audio Stream I/O API.
			WINDOWS_DS, //!< The Microsoft Direct Sound API.
			RTAUDIO_DUMMY, //!< A compilable but non-functional API.
			ANDROID_JAVA, //!< Android Interface.
			USER_INTERFACE_1, //!< User interface 1.
			USER_INTERFACE_2, //!< User interface 2.
			USER_INTERFACE_3, //!< User interface 3.
			USER_INTERFACE_4, //!< User interface 4.
		};
		
		extern const uint32_t MAX_SAMPLE_RATES;
		extern const uint32_t SAMPLE_RATES[];
		
		enum StreamState {
			STREAM_STOPPED,
			STREAM_STOPPING,
			STREAM_RUNNING,
			STREAM_CLOSED = -50
		};
		
		enum StreamMode {
			OUTPUT,
			INPUT,
			DUPLEX,
			UNINITIALIZED = -75
		};
		
		// A protected structure used for buffer conversion.
		struct ConvertInfo {
			int32_t channels;
			int32_t inJump, outJump;
			airtaudio::format inFormat, outFormat;
			std::vector<int> inOffset;
			std::vector<int> outOffset;
		};
		
		// A protected structure for audio streams.
		class Stream {
			public:
				uint32_t device[2]; // Playback and record, respectively.
				void *apiHandle; // void pointer for API specific stream handle information
				airtaudio::api::StreamMode mode; // OUTPUT, INPUT, or DUPLEX.
				airtaudio::api::StreamState state; // STOPPED, RUNNING, or CLOSED
				char *userBuffer[2]; // Playback and record, respectively.
				char *deviceBuffer;
				bool doConvertBuffer[2]; // Playback and record, respectively.
				bool userInterleaved;
				bool deviceInterleaved[2]; // Playback and record, respectively.
				bool doByteSwap[2]; // Playback and record, respectively.
				uint32_t sampleRate;
				uint32_t bufferSize;
				uint32_t nBuffers;
				uint32_t nUserChannels[2]; // Playback and record, respectively.
				uint32_t nDeviceChannels[2]; // Playback and record channels, respectively.
				uint32_t channelOffset[2]; // Playback and record, respectively.
				uint64_t latency[2]; // Playback and record, respectively.
				airtaudio::format userFormat;
				airtaudio::format deviceFormat[2]; // Playback and record, respectively.
				std::mutex mutex;
				airtaudio::CallbackInfo callbackInfo;
				airtaudio::api::ConvertInfo convertInfo[2];
				double streamTime; // Number of elapsed seconds since the stream started.
				
				#if defined(HAVE_GETTIMEOFDAY)
				struct timeval lastTickTimestamp;
				#endif
				
				Stream(void) :
				  apiHandle(0),
				  deviceBuffer(0) {
					device[0] = 11111;
					device[1] = 11111;
				}
		};
	};
	/**
	 * RtApi class declaration.
	 *
	 * Subclasses of RtApi contain all API- and OS-specific code necessary
	 * to fully implement the RtAudio API.
	 *
	 * Note that RtApi is an abstract base class and cannot be
	 * explicitly instantiated.	The class RtAudio will create an
	 * instance of an RtApi subclass (RtApiOss, RtApiAlsa,
	 * RtApiJack, RtApiCore, RtApiDs, or RtApiAsio).
	 */
	class Api {
		public:
			Api(void);
			virtual ~Api(void);
			virtual airtaudio::api::type getCurrentApi(void) = 0;
			virtual uint32_t getDeviceCount(void) = 0;
			virtual airtaudio::DeviceInfo getDeviceInfo(uint32_t _device) = 0;
			virtual uint32_t getDefaultInputDevice(void);
			virtual uint32_t getDefaultOutputDevice(void);
			enum airtaudio::errorType openStream(airtaudio::StreamParameters *_outputParameters,
			                                     airtaudio::StreamParameters *_inputParameters,
			                                     airtaudio::format _format,
			                                     uint32_t _sampleRate,
			                                     uint32_t *_bufferFrames,
			                                     airtaudio::AirTAudioCallback _callback,
			                                     void *_userData,
			                                     airtaudio::StreamOptions *_options);
			virtual enum airtaudio::errorType closeStream(void);
			virtual enum airtaudio::errorType startStream(void) = 0;
			virtual enum airtaudio::errorType stopStream(void) = 0;
			virtual enum airtaudio::errorType abortStream(void) = 0;
			long getStreamLatency(void);
			uint32_t getStreamSampleRate(void);
			virtual double getStreamTime(void);
			bool isStreamOpen(void) const {
				return m_stream.state != airtaudio::api::STREAM_CLOSED;
			}
			bool isStreamRunning(void) const {
				return m_stream.state == airtaudio::api::STREAM_RUNNING;
			}
			
		protected:
			airtaudio::api::Stream m_stream;
			
			/*!
				Protected, api-specific method that attempts to open a device
				with the given parameters.	This function MUST be implemented by
				all subclasses.	If an error is encountered during the probe, a
				"warning" message is reported and false is returned. A
				successful probe is indicated by a return value of true.
			*/
			virtual bool probeDeviceOpen(uint32_t _device,
			                             airtaudio::api::StreamMode _mode,
			                             uint32_t _channels,
			                             uint32_t _firstChannel,
			                             uint32_t _sampleRate,
			                             airtaudio::format _format,
			                             uint32_t *_bufferSize,
			                             airtaudio::StreamOptions *_options);
			
			//! A protected function used to increment the stream time.
			void tickStreamTime(void);
			
			//! Protected common method to clear an RtApiStream structure.
			void clearStreamInfo(void);
			
			/*!
				Protected common method that throws an RtError (type =
				INVALID_USE) if a stream is not open.
			*/
			enum airtaudio::errorType verifyStream(void);
			/**
			 * @brief Protected method used to perform format, channel number, and/or interleaving
			 * conversions between the user and device buffers.
			 */
			void convertBuffer(char *_outBuffer, char *_inBuffer, airtaudio::api::ConvertInfo &_info);
			
			//! Protected common method used to perform byte-swapping on buffers.
			void byteSwapBuffer(char *_buffer, uint32_t _samples, airtaudio::format _format);
			
			//! Protected common method that returns the number of bytes for a given format.
			uint32_t formatBytes(airtaudio::format _format);
			
			//! Protected common method that sets up the parameters for buffer conversion.
			void setConvertInfo(airtaudio::api::StreamMode _mode, uint32_t _firstChannel);
	};
};

#endif
