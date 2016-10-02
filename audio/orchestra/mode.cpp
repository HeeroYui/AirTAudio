/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <audio/orchestra/mode.hpp>
#include <audio/orchestra/debug.hpp>

int32_t audio::orchestra::modeToIdTable(enum mode _mode) {
	switch (_mode) {
		case mode_unknow:
		case mode_duplex:
		case mode_output:
			return 0;
		case mode_input:
			return 1;
	}
	return 0;
}

std::ostream& audio::operator <<(std::ostream& _os, enum audio::orchestra::mode _obj) {
	switch (_obj) {
		case audio::orchestra::mode_unknow:
			_os << "unknow";
			break;
		case audio::orchestra::mode_duplex:
			_os << "duplex";
			break;
		case audio::orchestra::mode_output:
			_os << "output";
			break;
		case audio::orchestra::mode_input:
			_os << "input";
			break;
	}
	return _os;
}