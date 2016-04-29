/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <audio/orchestra/status.h>
#include <audio/orchestra/debug.h>
static const char* listValue[] = {
	"ok",
	"overflow",
	"underflow"
};

std::ostream& audio::orchestra::operator <<(std::ostream& _os, enum audio::orchestra::status _obj) {
	_os << listValue[int32_t(_obj)];
	return _os;
}

std::ostream& audio::orchestra::operator <<(std::ostream& _os, const std::vector<enum audio::orchestra::status>& _obj) {
	_os << std::string("{");
	for (size_t iii=0; iii<_obj.size(); ++iii) {
		if (iii!=0) {
			_os << std::string(";");
		}
		_os << _obj[iii];
	}
	_os << std::string("}");
	return _os;
}

