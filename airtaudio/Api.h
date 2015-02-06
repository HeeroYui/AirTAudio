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
#include <airtaudio/debug.h>

namespace airtaudio {
	const std::vector<uint32_t>& genericSampleRate();
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
	std::ostream& operator <<(std::ostream& _os, const enum airtaudio::type& _obj);
	std::ostream& operator <<(std::ostream& _os, const std::vector<enum airtaudio::type>& _obj);
	std::string getTypeString(enum audio::format _value);
	enum airtaudio::type getTypeFromString(const std::string& _value);
	
	enum state {
		state_closed,
		state_stopped,
		state_stopping,
		state_running
	};
	enum mode {
		mode_unknow,
		mode_output,
		mode_input,
		mode_duplex
	};
	int32_t modeToIdTable(enum mode _mode);
	// A protected structure used for buffer conversion.
	class ConvertInfo {
		public:
			int32_t channels;
			int32_t inJump;
			int32_t outJump;
			enum audio::format inFormat;
			enum audio::format outFormat;
			std::vector<int> inOffset;
			std::vector<int> outOffset;
	};

	namespace api {
		// A protected structure for audio streams.
		class Stream {
			public:
				uint32_t device[2]; // Playback and record, respectively.
				void *apiHandle; // void pointer for API specific stream handle information
				enum airtaudio::mode mode; // airtaudio::mode_output, airtaudio::mode_input, or airtaudio::mode_duplex.
				enum airtaudio::state state; // STOPPED, RUNNING, or CLOSED
				char *userBuffer[2]; // Playback and record, respectively.
				char *deviceBuffer;
				bool doConvertBuffer[2]; // Playback and record, respectively.
				bool deviceInterleaved[2]; // Playback and record, respectively.
				bool doByteSwap[2]; // Playback and record, respectively.
				uint32_t sampleRate;
				uint32_t bufferSize;
				uint32_t nBuffers;
				uint32_t nUserChannels[2]; // Playback and record, respectively.
				uint32_t nDeviceChannels[2]; // Playback and record channels, respectively.
				uint32_t channelOffset[2]; // Playback and record, respectively.
				uint64_t latency[2]; // Playback and record, respectively.
				enum audio::format userFormat;
				enum audio::format deviceFormat[2]; // Playback and record, respectively.
				std::mutex mutex;
				airtaudio::CallbackInfo callbackInfo;
				airtaudio::ConvertInfo convertInfo[2];
				double streamTime; // Number of elapsed seconds since the stream started.
				
				#if defined(HAVE_GETTIMEOFDAY)
				struct timeval lastTickTimestamp;
				#endif
				
				Stream() :
				  apiHandle(nullptr),
				  deviceBuffer(nullptr) {
					device[0] = 11111;
					device[1] = 11111;
				}
		};
	};
	class Api {
		public:
			Api();
			virtual ~Api();
			virtual airtaudio::type getCurrentApi() = 0;
			virtual uint32_t getDeviceCount() = 0;
			virtual airtaudio::DeviceInfo getDeviceInfo(uint32_t _device) = 0;
			virtual uint32_t getDefaultInputDevice();
			virtual uint32_t getDefaultOutputDevice();
			enum airtaudio::error openStream(airtaudio::StreamParameters *_outputParameters,
			                                     airtaudio::StreamParameters *_inputParameters,
			                                     audio::format _format,
			                                     uint32_t _sampleRate,
			                                     uint32_t *_bufferFrames,
			                                     airtaudio::AirTAudioCallback _callback,
			                                     airtaudio::StreamOptions *_options);
			virtual enum airtaudio::error closeStream();
			virtual enum airtaudio::error startStream() = 0;
			virtual enum airtaudio::error stopStream() = 0;
			virtual enum airtaudio::error abortStream() = 0;
			long getStreamLatency();
			uint32_t getStreamSampleRate();
			virtual double getStreamTime();
			bool isStreamOpen() const {
				return m_stream.state != airtaudio::state_closed;
			}
			bool isStreamRunning() const {
				return m_stream.state == airtaudio::state_running;
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
			                             enum airtaudio::mode _mode,
			                             uint32_t _channels,
			                             uint32_t _firstChannel,
			                             uint32_t _sampleRate,
			                             enum audio::format _format,
			                             uint32_t *_bufferSize,
			                             airtaudio::StreamOptions *_options);
			
			//! A protected function used to increment the stream time.
			void tickStreamTime();
			
			//! Protected common method to clear an RtApiStream structure.
			void clearStreamInfo();
			
			/*!
				Protected common method that throws an RtError (type =
				INVALID_USE) if a stream is not open.
			*/
			enum airtaudio::error verifyStream();
			/**
			 * @brief Protected method used to perform format, channel number, and/or interleaving
			 * conversions between the user and device buffers.
			 */
			void convertBuffer(char *_outBuffer,
			                   char *_inBuffer,
			                   airtaudio::ConvertInfo& _info);
			
			//! Protected common method used to perform byte-swapping on buffers.
			void byteSwapBuffer(char *_buffer,
			                    uint32_t _samples,
			                    enum audio::format _format);
			
			//! Protected common method that sets up the parameters for buffer conversion.
			void setConvertInfo(enum airtaudio::mode _mode,
			                    uint32_t _firstChannel);
	};
};
/**
 * @brief Debug operator To display the curent element in a Human redeable information
 */
std::ostream& operator <<(std::ostream& _os, const airtaudio::type& _obj);

#endif
