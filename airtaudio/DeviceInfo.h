/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#ifndef __AIRTAUDIO_DEVICE_INFO_H__
#define __AIRTAUDIO_DEVICE_INFO_H__

namespace airtaudio {
	/**
	 * @brief The public device information structure for returning queried values.
	 */
	class DeviceInfo {
		public:
			bool probed; //!< true if the device capabilities were successfully probed.
			std::string name; //!< Character string device identifier.
			uint32_t outputChannels; //!< Maximum output channels supported by device.
			uint32_t inputChannels; //!< Maximum input channels supported by device.
			uint32_t duplexChannels; //!< Maximum simultaneous input/output channels supported by device.
			bool isDefaultOutput; //!< true if this is the default output device.
			bool isDefaultInput; //!< true if this is the default input device.
			std::vector<uint32_t> sampleRates; //!< Supported sample rates (queried from list of standard rates).
			airtaudio::format nativeFormats; //!< Bit mask of supported data formats.
			// Default constructor.
			DeviceInfo(void) :
			  probed(false),
			  outputChannels(0),
			  inputChannels(0),
			  duplexChannels(0),
			  isDefaultOutput(false),
			  isDefaultInput(false),
			  nativeFormats(0) {}
	};
};

#endif

