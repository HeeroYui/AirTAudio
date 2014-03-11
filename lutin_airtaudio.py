#!/usr/bin/python
import lutinModule as module
import lutinTools as tools
import lutinDebug as debug

def get_desc():
	return "airtaudio : Generic wrapper on all audio interface"


def create(target):
	myModule = module.Module(__file__, 'airtaudio', 'LIBRARY')
	
	myModule.add_src_file([
		'airtaudio/Interface.cpp',
		'airtaudio/Api.cpp',
		'airtaudio/api/Alsa.cpp',
		'airtaudio/api/Asio.cpp',
		'airtaudio/api/Core.cpp',
		'airtaudio/api/Ds.cpp',
		'airtaudio/api/Dummy.cpp',
		'airtaudio/api/Jack.cpp',
		'airtaudio/api/Oss.cpp',
		'airtaudio/api/Pulse.cpp'
		])
	
	if target.name=="Windows":
		# ASIO API on Windows
		myModule.add_export_flag_CC(['__WINDOWS_ASIO__'])
		# Windows DirectSound API
		#myModule.add_export_flag_CC(['__WINDOWS_DS__'])
	elif target.name=="Linux":
		# Linux Alsa API
		myModule.add_export_flag_CC(['-D__LINUX_ALSA__'])
		myModule.add_export_flag_LD("-lasound")
		# Linux Jack API
		myModule.add_export_flag_CC(['-D__UNIX_JACK__'])
		myModule.add_export_flag_LD("-ljack")
		# Linux PulseAudio API
		myModule.add_export_flag_CC(['-D__LINUX_PULSE__'])
		myModule.add_export_flag_LD("-lpulse-simple")
		myModule.add_export_flag_LD("-lpulse")
		#depending libs :
		myModule.add_export_flag_LD("-lpthread")
	elif target.name=="MacOs":
		# MacOsX core
		myModule.add_export_flag_CC(['__MACOSX_CORE__'])
		myModule.add_export_flag_LD("-framework CoreAudio")
		myModule.add_export_flag_LD("-framework CoreMIDI")
	else:
		debug.warning("unknow target for RTAudio : " + target.name);
	
	myModule.add_export_path(tools.get_current_path(__file__))
	myModule.add_path(tools.get_current_path(__file__)+"/rtaudio/")
	myModule.add_path(tools.get_current_path(__file__)+"/rtaudio/include/")
	
	# add the currrent module at the 
	return myModule









