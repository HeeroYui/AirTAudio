/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_STATUS_H__
#define __AIRTAUDIO_STATUS_H__

#include <etk/types.h>

namespace airtaudio {
	enum status {
		status_ok, //!< nothing...
		status_overflow, //!< Internal buffer has more data than they can accept
		status_underflow //!< The internal buffer is empty
	};
	std::ostream& operator <<(std::ostream& _os, enum airtaudio::status _obj);
	std::ostream& operator <<(std::ostream& _os, const std::vector<enum airtaudio::status>& _obj);
	
};

#endif
