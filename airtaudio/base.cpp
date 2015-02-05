/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */
#if 0
#include <airtaudio/base.h>

std::ostream& airtaudio::operator <<(std::ostream& _os, enum errorType _obj) {
	switch(_obj) {
		case errorNone:
			_os << "errorNone";
			break;
		case errorFail:
			_os << "errorFail";
			break;
		case errorWarning:
			_os << "errorWarning";
			break;
		case errorInputNull:
			_os << "errorInputNull";
			break;
		case errorInvalidUse:
			_os << "errorInvalidUse";
			break;
		case errorSystemError:
			_os << "errorSystemError";
			break;
		default:
			_os << "UNKNOW...";
			break;
	}
	return _os;
}

std::ostream& airtaudio::operator <<(std::ostream& _os, const audio::format& _obj) {
	switch(_obj) {
		case SINT8:
			_os << "SINT8";
			break;
		case SINT16:
			_os << "SINT16";
			break;
		case SINT24:
			_os << "SINT24";
			break;
		case SINT32:
			_os << "SINT32";
			break;
		case FLOAT32:
			_os << "FLOAT32";
			break;
		case FLOAT64:
			_os << "FLOAT64";
			break;
		default:
			_os << "UNKNOW...";
			break;
	}
	return _os;
}

std::ostream& airtaudio::operator <<(std::ostream& _os, const airtaudio::streamFlags& _obj) {
	switch(_obj) {
		case NONINTERLEAVED:
			_os << "NONINTERLEAVED";
			break;
		case MINIMIZE_LATENCY:
			_os << "MINIMIZE_LATENCY";
			break;
		case HOG_DEVICE:
			_os << "HOG_DEVICE";
			break;
		case SCHEDULE_REALTIME:
			_os << "SCHEDULE_REALTIME";
			break;
		case ALSA_USE_DEFAULT:
			_os << "ALSA_USE_DEFAULT";
			break;
		default:
			_os << "UNKNOW...";
			break;
	}
	return _os;
}

std::ostream& airtaudio::operator <<(std::ostream& _os, const airtaudio::streamStatus& _obj) {
	switch(_obj) {
		case INPUT_OVERFLOW:
			_os << "INPUT_OVERFLOW";
			break;
		case OUTPUT_UNDERFLOW:
			_os << "OUTPUT_UNDERFLOW";
			break;
		default:
			_os << "UNKNOW...";
			break;
	}
	return _os;
}


#endif
