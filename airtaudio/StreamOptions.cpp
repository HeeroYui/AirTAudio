/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <airtaudio/StreamOptions.h>
#include <etk/stdTools.h>
#include <airtaudio/debug.h>

static const char* listValue[] = {
	"hardware",
	"trigered",
	"soft"
};

std::ostream& airtaudio::operator <<(std::ostream& _os, enum airtaudio::timestampMode _obj) {
	_os << listValue[_obj];
	return _os;
}

namespace etk {
	template <> bool from_string<enum airtaudio::timestampMode>(enum airtaudio::timestampMode& _variableRet, const std::string& _value) {
		if (_value == "hardware") {
			_variableRet = airtaudio::timestampMode_Hardware;
			return true;
		}
		if (_value == "trigered") {
			_variableRet = airtaudio::timestampMode_trigered;
			return true;
		}
		if (_value == "soft") {
			_variableRet = airtaudio::timestampMode_soft;
			return true;
		}
		return false;
	}
	
	template <enum airtaudio::timestampMode> std::string to_string(const enum airtaudio::timestampMode& _variable) {
		return listValue[_variable];
	}
}


