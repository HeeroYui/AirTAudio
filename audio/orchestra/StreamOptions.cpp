/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <audio/orchestra/StreamOptions.hpp>
#include <etk/stdTools.hpp>
#include <audio/orchestra/debug.hpp>

static const char* listValue[] = {
	"hardware",
	"trigered",
	"soft"
};

etk::Stream& audio::orchestra::operator <<(etk::Stream& _os, enum audio::orchestra::timestampMode _obj) {
	_os << listValue[_obj];
	return _os;
}

namespace etk {
	template <> bool from_string<enum audio::orchestra::timestampMode>(enum audio::orchestra::timestampMode& _variableRet, const etk::String& _value) {
		if (_value == "hardware") {
			_variableRet = audio::orchestra::timestampMode_Hardware;
			return true;
		}
		if (_value == "trigered") {
			_variableRet = audio::orchestra::timestampMode_trigered;
			return true;
		}
		if (_value == "soft") {
			_variableRet = audio::orchestra::timestampMode_soft;
			return true;
		}
		return false;
	}
	
	template <enum audio::orchestra::timestampMode> etk::String toString(const enum audio::orchestra::timestampMode& _variable) {
		return listValue[_variable];
	}
}


