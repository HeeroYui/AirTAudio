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
	ATA_PRINT(space + "probe=" << probed);
	ATA_PRINT(space + "name=" << name);
	ATA_PRINT(space + "outputChannels=" << outputChannels);
	ATA_PRINT(space + "inputChannels=" << inputChannels);
	ATA_PRINT(space + "duplexChannels=" << duplexChannels);
	ATA_PRINT(space + "isDefaultOutput=" << (isDefaultOutput==true?"true":"false"));
	ATA_PRINT(space + "isDefaultInput=" << (isDefaultInput==true?"true":"false"));
	ATA_PRINT(space + "rates=" << sampleRates);
	ATA_PRINT(space + "native Format: " << nativeFormats);
}

std::ostream& audio::orchestra::operator <<(std::ostream& _os, const audio::orchestra::DeviceInfo& _obj) {
	_os << "{";
	_os << "probe=" << _obj.probed << ", ";
	_os << "name=" << _obj.name << ", ";
	_os << "outputChannels=" << _obj.outputChannels << ", ";
	_os << "inputChannels=" << _obj.inputChannels << ", ";
	_os << "duplexChannels=" << _obj.duplexChannels << ", ";
	_os << "isDefaultOutput=" << _obj.isDefaultOutput << ", ";
	_os << "isDefaultInput=" << _obj.isDefaultInput << ", ";
	_os << "rates=" << _obj.sampleRates << ", ";
	_os << "native Format: " << _obj.nativeFormats;
	_os << "}";
	return _os;
}

