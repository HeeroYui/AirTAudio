/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_API_H__
#define __AIRTAUDIO_API_H__

#include <sstream>
#include <airtaudio/debug.h>
#include <airtaudio/type.h>
#include <airtaudio/state.h>
#include <airtaudio/mode.h>

namespace airtaudio {
	const std::vector<uint32_t>& genericSampleRate();
	
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

	class Api {
		public:
			Api();
			virtual ~Api();
			virtual airtaudio::type getCurrentApi() = 0;
			virtual uint32_t getDeviceCount() = 0;
			virtual airtaudio::DeviceInfo getDeviceInfo(uint32_t _device) = 0;
			virtual uint32_t getDefaultInputDevice();
			virtual uint32_t getDefaultOutputDevice();
			enum airtaudio::error openStream(airtaudio::StreamParameters* _outputParameters,
			                                 airtaudio::StreamParameters* _inputParameters,
			                                 audio::format _format,
			                                 uint32_t _sampleRate,
			                                 uint32_t* _bufferFrames,
			                                 airtaudio::AirTAudioCallback _callback,
			                                 airtaudio::StreamOptions* _options);
			virtual enum airtaudio::error closeStream();
			virtual enum airtaudio::error startStream() = 0;
			virtual enum airtaudio::error stopStream() = 0;
			virtual enum airtaudio::error abortStream() = 0;
			long getStreamLatency();
			uint32_t getStreamSampleRate();
			virtual double getStreamTime();
			bool isStreamOpen() const {
				return m_state != airtaudio::state_closed;
			}
			bool isStreamRunning() const {
				return m_state == airtaudio::state_running;
			}
			
		protected:
			mutable std::mutex m_mutex;
			uint32_t m_device[2]; // Playback and record, respectively.
			enum airtaudio::mode m_mode; // airtaudio::mode_output, airtaudio::mode_input, or airtaudio::mode_duplex.
			enum airtaudio::state m_state; // STOPPED, RUNNING, or CLOSED
			std::vector<char> m_userBuffer[2]; // Playback and record, respectively.
			char *m_deviceBuffer;
			bool m_doConvertBuffer[2]; // Playback and record, respectively.
			bool m_deviceInterleaved[2]; // Playback and record, respectively.
			bool m_doByteSwap[2]; // Playback and record, respectively.
			uint32_t m_sampleRate;
			uint32_t m_bufferSize;
			uint32_t m_nBuffers;
			uint32_t m_nUserChannels[2]; // Playback and record, respectively.
			uint32_t m_nDeviceChannels[2]; // Playback and record channels, respectively.
			uint32_t m_channelOffset[2]; // Playback and record, respectively.
			uint64_t m_latency[2]; // Playback and record, respectively.
			enum audio::format m_userFormat; // TODO : Remove this ==> use can only open in the Harware format ...
			enum audio::format m_deviceFormat[2]; // Playback and record, respectively.
			// TODO : Remove this ...
			airtaudio::CallbackInfo m_callbackInfo;
			airtaudio::ConvertInfo m_convertInfo[2];
			// TODO : use : std::chrono::system_clock::time_point ...
			double m_streamTime; // Number of elapsed seconds since the stream started.
			
			/**
			 * @brief api-specific method that attempts to open a device
			 * with the given parameters. This function MUST be implemented by
			 * all subclasses. If an error is encountered during the probe, a
			 * "warning" message is reported and false is returned. A
			 * successful probe is indicated by a return value of true.
			 */
			virtual bool probeDeviceOpen(uint32_t _device,
			                             enum airtaudio::mode _mode,
			                             uint32_t _channels,
			                             uint32_t _firstChannel,
			                             uint32_t _sampleRate,
			                             enum audio::format _format,
			                             uint32_t *_bufferSize,
			                             airtaudio::StreamOptions *_options);
			/**
			 * @brief Increment the stream time.
			 */
			void tickStreamTime();
			/**
			 * @brief Clear an RtApiStream structure.
			 */
			void clearStreamInfo();
			/**
			 * @brief Check the current stream status
			 */
			enum airtaudio::error verifyStream();
			/**
			 * @brief Protected method used to perform format, channel number, and/or interleaving
			 * conversions between the user and device buffers.
			 */
			void convertBuffer(char *_outBuffer,
			                   char *_inBuffer,
			                   airtaudio::ConvertInfo& _info);
			
			/**
			 * @brief Perform byte-swapping on buffers.
			 */
			void byteSwapBuffer(char *_buffer,
			                    uint32_t _samples,
			                    enum audio::format _format);
			/**
			 * @brief Sets up the parameters for buffer conversion.
			 */
			void setConvertInfo(enum airtaudio::mode _mode,
			                    uint32_t _firstChannel);
	};
};
/**
 * @brief Debug operator To display the curent element in a Human redeable information
 */
std::ostream& operator <<(std::ostream& _os, const airtaudio::type& _obj);

#endif
