/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

//#include <etk/types.h>
#include <audio/orchestra/debug.h>
#include <audio/orchestra/DeviceInfo.h>
#include <etk/stdTools.h>
#include <iostream>

#undef __class__
#define __class__ "DeviceInfo"

void audio::orchestra::DeviceInfo::display(int32_t _tabNumber) const {
	std::string space;
	for (int32_t iii=0; iii<_tabNumber; ++iii) {
		space += "    ";
	}
	ATA_PRINT(space + "mode=" << (input==true?"input":"output"));
	ATA_PRINT(space + "name=" << name);
	ATA_PRINT(space + "desc=" << desc);
	ATA_PRINT(space + "channel" << (channels.size()>1?"s":"") << "=" << channels.size() << " : " << channels);
	ATA_PRINT(space + "rate" << (sampleRates.size()>1?"s":"") << "=" << sampleRates);
	ATA_PRINT(space + "native Format" << (nativeFormats.size()>1?"s":"") << ": " << nativeFormats);
	ATA_PRINT(space + "default=" << (isDefault==true?"true":"false"));
}


std::ostream& audio::orchestra::operator <<(std::ostream& _os, const audio::orchestra::DeviceInfo& _obj) {
	_os << "{";
	_os << "name=" << _obj.name << ", ";
	_os << "description=" << _obj.desc << ", ";
	_os << "channels=" << _obj.channels << ", ";
	_os << "default=" << _obj.isDefault << ", ";
	_os << "rates=" << _obj.sampleRates << ", ";
	_os << "native Format: " << _obj.nativeFormats;
	_os << "}";
	return _os;
}

