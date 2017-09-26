/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

//#include <etk/types.hpp>
#include <audio/orchestra/debug.hpp>
#include <audio/orchestra/DeviceInfo.hpp>
#include <etk/stdTools.hpp>

void audio::orchestra::DeviceInfo::display(int32_t _tabNumber) const {
	etk::String space;
	for (int32_t iii=0; iii<_tabNumber; ++iii) {
		space += "    ";
	}
	if (isCorrect == false) {
		ATA_PRINT(space + "NOT CORRECT INFORAMATIONS");
		return;
	}
	ATA_PRINT(space + "mode=" << (input==true?"input":"output"));
	ATA_PRINT(space + "name=" << name);
	if (desc.size() != 0) {
		ATA_PRINT(space + "desc=" << desc);
	}
	ATA_PRINT(space + "channel" << (channels.size()>1?"s":"") << "=" << channels.size() << " : " << channels);
	ATA_PRINT(space + "rate" << (sampleRates.size()>1?"s":"") << "=" << sampleRates);
	ATA_PRINT(space + "native Format" << (nativeFormats.size()>1?"s":"") << ": " << nativeFormats);
	ATA_PRINT(space + "default=" << (isDefault==true?"true":"false"));
}

void audio::orchestra::DeviceInfo::clear() {
	isCorrect = false;
	input = false;
	name = "";
	desc = "";
	channels.clear();
	sampleRates.clear();
	nativeFormats.clear();
	isDefault = false;
}

etk::Stream& audio::orchestra::operator <<(etk::Stream& _os, const audio::orchestra::DeviceInfo& _obj) {
	_os << "{";
	if (_obj.isCorrect == false) {
		_os << "NOT CORRECT INFORAMATIONS";
	} else {
		_os << "name=" << _obj.name << ", ";
		if (_obj.desc.size() != 0) {
			_os << "description=" << _obj.desc << ", ";
		}
		_os << "channels=" << _obj.channels << ", ";
		_os << "default=" << _obj.isDefault << ", ";
		_os << "rates=" << _obj.sampleRates << ", ";
		_os << "native Format: " << _obj.nativeFormats;
	}
	_os << "}";
	return _os;
}

