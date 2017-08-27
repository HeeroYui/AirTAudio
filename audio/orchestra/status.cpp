/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <audio/orchestra/status.hpp>
#include <audio/orchestra/debug.hpp>
static const char* listValue[] = {
	"ok",
	"overflow",
	"underflow"
};

etk::Stream& audio::orchestra::operator <<(etk::Stream& _os, enum audio::orchestra::status _obj) {
	_os << listValue[int32_t(_obj)];
	return _os;
}

etk::Stream& audio::orchestra::operator <<(etk::Stream& _os, const etk::Vector<enum audio::orchestra::status>& _obj) {
	_os << etk::String("{");
	for (size_t iii=0; iii<_obj.size(); ++iii) {
		if (iii!=0) {
			_os << etk::String(";");
		}
		_os << _obj[iii];
	}
	_os << etk::String("}");
	return _os;
}

