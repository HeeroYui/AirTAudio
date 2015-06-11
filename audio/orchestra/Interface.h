/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AUDIO_ORCHESTRA_INTERFACE_H__
#define __AUDIO_ORCHESTRA_INTERFACE_H__

#include <string>
#include <vector>
#include <audio/orchestra/base.h>
#include <audio/orchestra/CallbackInfo.h>
#include <audio/orchestra/Api.h>
#include <audio/orchestra/api/Alsa.h>
#include <audio/orchestra/api/Android.h>
#include <audio/orchestra/api/Asio.h>
#include <audio/orchestra/api/Core.h>
#include <audio/orchestra/api/CoreIos.h>
#include <audio/orchestra/api/Ds.h>
#include <audio/orchestra/api/Dummy.h>
#include <audio/orchestra/api/Jack.h>
#include <audio/orchestra/api/Oss.h>
#include <audio/orchestra/api/Pulse.h>

namespace audio {
	namespace orchestra {
		/**
		 * @brief audio::orchestra::Interface class declaration.
		 *
		 * audio::orchestra::Interface is a "controller" used to select an available audio i/o
		 * interface.	It presents a common API for the user to call but all
		 * functionality is implemented by the class RtApi and its
		 * subclasses.	RtAudio creates an instance of an RtApi subclass
		 * based on the user's API choice.	If no choice is made, RtAudio
		 * attempts to make a "logical" API selection.
		 */
		class Interface {
			protected:
				std::vector<std::pair<std::string, Api* (*)()> > m_apiAvaillable;
			protected:
				audio::orchestra::Api *m_rtapi;
			public:
				void setName(const std::string& _name) {
					if (m_rtapi == nullptr) {
						
						return;
					}
					m_rtapi->setName(_name);
				}
				/**
				 * @brief Get the list of all availlable API in the system.
				 * @return the list of all APIs
				 */
				std::vector<std::string> getListApi();
				/**
				 * @brief Add an interface of the Possible List.
				 * @param[in] _api Type of the interface.
				 * @param[in] _callbackCreate API creation callback.
				 */
				void addInterface(const std::string& _api, Api* (*_callbackCreate)());
				/**
				 * @brief The class constructor.
				 * @note the creating of the basic instance is done by Instanciate
				 */
				Interface();
				/**
				 * @brief The destructor.
				 * 
				 * If a stream is running or open, it will be stopped and closed
				 * automatically.
				 */
				virtual ~Interface();
				/**
				 * @brief Clear the current Interface
				 */
				enum audio::orchestra::error clear();
				/**
				 * @brief Create an interface instance
				 */
				enum audio::orchestra::error instanciate(const std::string& _api = audio::orchestra::type_undefined);
				/**
				 * @return the audio API specifier for the current instance of airtaudio.
				 */
				const std::string& getCurrentApi() {
					if (m_rtapi == nullptr) {
						return audio::orchestra::type_undefined;
					}
					return m_rtapi->getCurrentApi();
				}
				/**
				 * @brief A public function that queries for the number of audio devices available.
				 * 
				 * This function performs a system query of available devices each time it
				 * is called, thus supporting devices connected \e after instantiation. If
				 * a system error occurs during processing, a warning will be issued. 
				 */
				uint32_t getDeviceCount() {
					if (m_rtapi == nullptr) {
						return 0;
					}
					return m_rtapi->getDeviceCount();
				}
				/**
				 * @brief Any device integer between 0 and getDeviceCount() - 1 is valid.
				 * If an invalid argument is provided, an RtError (type = INVALID_USE)
				 * will be thrown.	If a device is busy or otherwise unavailable, the
				 * structure member "probed" will have a value of "false" and all
				 * other members are undefined.	If the specified device is the
				 * current default input or output device, the corresponding
				 * "isDefault" member will have a value of "true".
				 *
				 * @return An audio::orchestra::DeviceInfo structure for a specified device number.
				 */
				audio::orchestra::DeviceInfo getDeviceInfo(uint32_t _device) {
					if (m_rtapi == nullptr) {
						return audio::orchestra::DeviceInfo();
					}
					return m_rtapi->getDeviceInfo(_device);
				}
				audio::orchestra::DeviceInfo getDeviceInfo(const std::string& _deviceName) {
					if (m_rtapi == nullptr) {
						return audio::orchestra::DeviceInfo();
					}
					audio::orchestra::DeviceInfo info;
					m_rtapi->getNamedDeviceInfo(_deviceName, info);
					return info;
				}
				/**
				 * @brief A function that returns the index of the default output device.
				 * 
				 * If the underlying audio API does not provide a "default
				 * device", or if no devices are available, the return value will be
				 * 0. Note that this is a valid device identifier and it is the
				 * client's responsibility to verify that a device is available
				 * before attempting to open a stream.
				 */
				uint32_t getDefaultOutputDevice() {
					if (m_rtapi == nullptr) {
						return 0;
					}
					return m_rtapi->getDefaultOutputDevice();
				}
				/**
				 * @brief A function that returns the index of the default input device.
				 * 
				 * If the underlying audio API does not provide a "default
				 * device", or if no devices are available, the return value will be
				 * 0. Note that this is a valid device identifier and it is the
				 * client's responsibility to verify that a device is available
				 * before attempting to open a stream.
				 */
				uint32_t getDefaultInputDevice() {
					if (m_rtapi == nullptr) {
						return 0;
					}
					return m_rtapi->getDefaultInputDevice();
				}
				/**
				 * @brief A public function for opening a stream with the specified parameters.
				 * 
				 * An RtError (type = SYSTEM_ERROR) is thrown if a stream cannot be
				 * opened with the specified parameters or an error occurs during
				 * processing.	An RtError (type = INVALID_USE) is thrown if any
				 * invalid device ID or channel number parameters are specified.
				 * @param _outputParameters Specifies output stream parameters to use
				 *           when opening a stream, including a device ID, number of channels,
				 *           and starting channel number. For input-only streams, this
				 *           argument should be nullptr. The device ID is an index value between
				 *           0 and getDeviceCount() - 1.
				 * @param  _inputParameters Specifies input stream parameters to use
				 *           when opening a stream, including a device ID, number of channels,
				 *           and starting channel number. For output-only streams, this
				 *           argument should be nullptr. The device ID is an index value between
				 *           0 and getDeviceCount() - 1.
				 * @param  _format An audio::format specifying the desired sample data format.
				 * @param  _sampleRate The desired sample rate (sample frames per second).
				 * @param  _bufferFrames A pointer to a value indicating the desired
				 *           internal buffer size in sample frames.	The actual value
				 *           used by the device is returned via the same pointer. A
				 *           value of zero can be specified, in which case the lowest
				 *           allowable value is determined.
				 * @param  _callback A client-defined function that will be invoked
				 *           when input data is available and/or output data is needed.
				 * @param  _options An optional pointer to a structure containing various
				 *           global stream options, including a list of OR'ed audio::orchestra::streamFlags
				 *           and a suggested number of stream buffers that can be used to 
				 *           control stream latency. More buffers typically result in more
				 *           robust performance, though at a cost of greater latency. If a
				 *           value of zero is specified, a system-specific median value is
				 *           chosen. If the airtaudio_MINIMIZE_LATENCY flag bit is set, the
				 *           lowest allowable value is used. The actual value used is
				 *           returned via the structure argument. The parameter is API dependent.
				 * @param  _errorCallback A client-defined function that will be invoked
				 *           when an error has occured.
				 */
				enum audio::orchestra::error openStream(audio::orchestra::StreamParameters *_outputParameters,
				                                 audio::orchestra::StreamParameters *_inputParameters,
				                                 enum audio::format _format,
				                                 uint32_t _sampleRate,
				                                 uint32_t* _bufferFrames,
				                                 audio::orchestra::AirTAudioCallback _callback,
				                                 const audio::orchestra::StreamOptions& _options = audio::orchestra::StreamOptions());
				
				/**
				 * @brief A function that closes a stream and frees any associated stream memory.
				 * 
				 * If a stream is not open, this function issues a warning and
				 * returns (no exception is thrown).
				 */
				enum audio::orchestra::error closeStream() {
					if (m_rtapi == nullptr) {
						return audio::orchestra::error_inputNull;
					}
					return m_rtapi->closeStream();
				}
				/**
				 * @brief A function that starts a stream.
				 *
				 * An RtError (type = SYSTEM_ERROR) is thrown if an error occurs
				 * during processing.	An RtError (type = INVALID_USE) is thrown if a
				 * stream is not open.	A warning is issued if the stream is already
				 * running.
				 */
				enum audio::orchestra::error startStream() {
					if (m_rtapi == nullptr) {
						return audio::orchestra::error_inputNull;
					}
					return m_rtapi->startStream();
				}
				/**
				 * @brief Stop a stream, allowing any samples remaining in the output queue to be played.
				 * 
				 * An RtError (type = SYSTEM_ERROR) is thrown if an error occurs
				 * during processing.	An RtError (type = INVALID_USE) is thrown if a
				 * stream is not open.	A warning is issued if the stream is already
				 * stopped.
				*/
				enum audio::orchestra::error stopStream() {
					if (m_rtapi == nullptr) {
						return audio::orchestra::error_inputNull;
					}
					return m_rtapi->stopStream();
				}
				/**
				 * @brief Stop a stream, discarding any samples remaining in the input/output queue.
				 * An RtError (type = SYSTEM_ERROR) is thrown if an error occurs
				 * during processing.	An RtError (type = INVALID_USE) is thrown if a
				 * stream is not open.	A warning is issued if the stream is already
				 * stopped.
				 */
				enum audio::orchestra::error abortStream() {
					if (m_rtapi == nullptr) {
						return audio::orchestra::error_inputNull;
					}
					return m_rtapi->abortStream();
				}
				/**
				 * @return true if a stream is open and false if not.
				 */
				bool isStreamOpen() const {
					if (m_rtapi == nullptr) {
						return false;
					}
					return m_rtapi->isStreamOpen();
				}
				/**
				 * @return true if the stream is running and false if it is stopped or not open.
				 */
				bool isStreamRunning() const {
					if (m_rtapi == nullptr) {
						return false;
					}
					return m_rtapi->isStreamRunning();
				}
				/**
				 * @brief If a stream is not open, an RtError (type = INVALID_USE) will be thrown.
				 * @return the number of elapsed seconds since the stream was started.
				 */
				audio::Time getStreamTime() {
					if (m_rtapi == nullptr) {
						return audio::Time();
					}
					return m_rtapi->getStreamTime();
				}
				/**
				 * @brief The stream latency refers to delay in audio input and/or output
				 * caused by internal buffering by the audio system and/or hardware.
				 * For duplex streams, the returned value will represent the sum of
				 * the input and output latencies.	If a stream is not open, an
				 * RtError (type = INVALID_USE) will be thrown.	If the API does not
				 * report latency, the return value will be zero.
				 * @return The internal stream latency in sample frames.
				 */
				long getStreamLatency() {
					if (m_rtapi == nullptr) {
						return 0;
					}
					return m_rtapi->getStreamLatency();
				}
				/**
				 * @brief On some systems, the sample rate used may be slightly different
				 * than that specified in the stream parameters. If a stream is not
				 * open, an RtError (type = INVALID_USE) will be thrown.
				 * @return Returns actual sample rate in use by the stream.
				 */
				uint32_t getStreamSampleRate() {
					if (m_rtapi == nullptr) {
						return 0;
					}
					return m_rtapi->getStreamSampleRate();
				}
				bool isMasterOf(audio::orchestra::Interface& _interface);
			protected:
				void openApi(const std::string& _api);
		};
	}
}

#endif
