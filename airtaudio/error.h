/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_ERROR_H__
#define __AIRTAUDIO_ERROR_H__

#include <etk/types.h>

namespace airtaudio {
	enum error {
		error_none, //!< No error
		error_fail, //!< An error occure in the operation
		error_warning, //!< A non-critical error.
		error_inputNull, //!< null input or internal errror
		error_invalidUse, //!< The function was called incorrectly.
		error_systemError //!< A system error occured.
	};
};

#endif
