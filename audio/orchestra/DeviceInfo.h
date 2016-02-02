/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#pragma once

#include <audio/format.h>
#include <audio/channel.h>

namespace audio {
	namespace orchestra {
		/**
		 * @brief The public device information structure for returning queried values.
		 */
		class DeviceInfo {
			public:
				bool isCorrect; //!< the information is correct (the system can return information incorect).
				bool input; //!< true if the device in an input; false: output.
				std::string name; //!< Character string device identifier.
				std::string desc; //!< description of the device
				std::vector<audio::channel> channels; //!< Channels interfaces.
				std::vector<uint32_t> sampleRates; //!< Supported sample rates (queried from list of standard rates).
				std::vector<audio::format> nativeFormats; //!< Bit mask of supported data formats.
				bool isDefault; //! is default input/output
				// Default constructor.
				DeviceInfo() :
				  isCorrect(false),
				  input(false),
				  name(),
				  desc(),
				  channels(),
				  sampleRates(),
				  nativeFormats(),
				  isDefault(false) {}
				/**
				 * @brief Display the current information of the device (on console)
				 */
				void display(int32_t _tabNumber = 1) const;
				/**
				 * @brief Clear all internal data
				 */
				void clear();
		};
		std::ostream& operator <<(std::ostream& _os, const audio::orchestra::DeviceInfo& _obj);
	}
}

