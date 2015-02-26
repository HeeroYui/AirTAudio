/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

//#include <etk/types.h>
#include <airtaudio/debug.h>
#include <airtaudio/DeviceInfo.h>
#include <etk/stdTools.h>
#include <iostream>

#undef __class__
#define __class__ "DeviceInfo"

void airtaudio::DeviceInfo::display(int32_t _tabNumber) const {
	std::string space;
	for (int32_t iii=0; iii<_tabNumber; ++iii) {
		space += "    ";
	}
	ATA_INFO(space + "probe=" << probed);
	ATA_INFO(space + "name=" << name);
	ATA_INFO(space + "outputChannels=" << outputChannels);
	ATA_INFO(space + "inputChannels=" << inputChannels);
	ATA_INFO(space + "duplexChannels=" << duplexChannels);
	ATA_INFO(space + "isDefaultOutput=" << (isDefaultOutput==true?"true":"false"));
	ATA_INFO(space + "isDefaultInput=" << (isDefaultInput==true?"true":"false"));
	ATA_INFO(space + "rates=" << sampleRates);
	ATA_INFO(space + "native Format: " << nativeFormats);
}

std::ostream& airtaudio::operator <<(std::ostream& _os, const airtaudio::DeviceInfo& _obj) {
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

