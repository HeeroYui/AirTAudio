/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AUDIO_ORCHESTRA_DEVICE_INFO_H__
#define __AUDIO_ORCHESTRA_DEVICE_INFO_H__

#include <audio/format.h>


namespace audio {
	namespace orchestra {
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
				std::vector<audio::format> nativeFormats; //!< Bit mask of supported data formats.
				// Default constructor.
				DeviceInfo() :
				  probed(false),
				  outputChannels(0),
				  inputChannels(0),
				  duplexChannels(0),
				  isDefaultOutput(false),
				  isDefaultInput(false),
				  nativeFormats() {}
				void display(int32_t _tabNumber = 1) const;
		};
		std::ostream& operator <<(std::ostream& _os, const audio::orchestra::DeviceInfo& _obj);
	}
}

#endif

