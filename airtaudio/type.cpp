/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <airtaudio/type.h>
#include <airtaudio/debug.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <climits>

#undef __class__
#define __class__ "type"

static const char* listType[] = {
	"undefined",
	"alsa",
	"pulse",
	"oss",
	"jack",
	"coreOSX",
	"corIOS",
	"asio",
	"ds",
	"java",
	"dummy",
	"user1",
	"user2",
	"user3",
	"user4"
};
static int32_t listTypeSize = sizeof(listType)/sizeof(char*);


std::ostream& airtaudio::operator <<(std::ostream& _os, const enum airtaudio::type& _obj) {
	_os << listType[_obj];
	return _os;
}

std::ostream& airtaudio::operator <<(std::ostream& _os, const std::vector<enum airtaudio::type>& _obj) {
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
/*
template <enum audio::format> std::string to_string(const enum audio::format& _variable) {
	return listType[_value];
}
*/
std::string airtaudio::getTypeString(enum airtaudio::type _value) {
	return listType[_value];
}

enum airtaudio::type airtaudio::getTypeFromString(const std::string& _value) {
	for (int32_t iii=0; iii<listTypeSize; ++iii) {
		if (_value == listType[iii]) {
			return static_cast<enum airtaudio::type>(iii);
		}
	}
	if (_value == "auto") {
		return airtaudio::type_undefined;
	}
	return airtaudio::type_undefined;
}
