/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_STREAM_PARAMETER_H__
#define __AIRTAUDIO_STREAM_PARAMETER_H__

namespace airtaudio {
	/**
	 * @brief The structure for specifying input or ouput stream parameters.
	 */
	class StreamParameters {
		public:
			int32_t deviceId; //!< Device index (-1 to getDeviceCount() - 1).
			std::string deviceName; //!< name of the device (if deviceId==-1 this must not be == "", and the oposite ...)
			uint32_t nChannels; //!< Number of channels.
			uint32_t firstChannel; //!< First channel index on device (default = 0).
			// Default constructor.
			StreamParameters() :
			  deviceId(-1),
			  nChannels(0),
			  firstChannel(0) { }
	};
};

#endif

