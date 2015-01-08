#!/usr/bin/python
import lutinModule as module
import lutinTools as tools
import lutinDebug as debug

def get_desc():
	return "airtaudio : Generic wrapper on all audio interface"


def create(target):
	myModule = module.Module(__file__, 'airtaudio', 'LIBRARY')
	
	myModule.add_src_file([
		'airtaudio/debug.cpp',
		'airtaudio/base.cpp',
		'airtaudio/Interface.cpp',
		'airtaudio/Api.cpp',
		'airtaudio/api/Dummy.cpp',
		])
	
	myModule.add_export_flag_CC(['-D__AIRTAUDIO_API_DUMMY_H__'])
	if target.name=="Windows":
		myModule.add_src_file([
							   'airtaudio/api/Asio.cpp',
							   'airtaudio/api/Ds.cpp',
							   ])
		# ASIO API on Windows
		myModule.add_export_flag_CC(['__WINDOWS_ASIO__'])
		# Windows DirectSound API
		#myModule.add_export_flag_CC(['__WINDOWS_DS__'])
		myModule.add_module_depend(['etk'])
	elif target.name=="Linux":
		myModule.add_src_file([
							   'airtaudio/api/Alsa.cpp',
							   'airtaudio/api/Jack.cpp',
							   'airtaudio/api/Pulse.cpp',
							   'airtaudio/api/Oss.cpp'
							   ])
		# Linux Alsa API
		myModule.add_export_flag_CC(['-D__LINUX_ALSA__'])
		myModule.add_export_flag_LD("-lasound")
		# Linux Jack API
		#myModule.add_export_flag_CC(['-D__UNIX_JACK__'])
		#myModule.add_export_flag_LD("-ljack")
		# Linux PulseAudio API
		#myModule.add_export_flag_CC(['-D__LINUX_PULSE__'])
		#myModule.add_export_flag_LD("-lpulse-simple")
		#myModule.add_export_flag_LD("-lpulse")
		#myModule.add_export_flag_CC(['-D__LINUX_OSS__'])
		# ...
		myModule.add_module_depend(['etk'])
	elif target.name=="MacOs":
		myModule.add_src_file([
							   'airtaudio/api/Core.cpp',
							   'airtaudio/api/Oss.cpp'
							   ])
		# MacOsX core
		myModule.add_export_flag_CC(['-D__MACOSX_CORE__'])
		myModule.add_export_flag_LD("-framework CoreAudio")
		myModule.add_module_depend(['etk'])
	elif target.name=="IOs":
		myModule.add_src_file('airtaudio/api/CoreIos.mm')
		# IOsX core
		myModule.add_export_flag_CC(['-D__IOS_CORE__'])
		myModule.add_export_flag_LD("-framework CoreAudio")
		myModule.add_export_flag_LD("-framework AudioToolbox")
		myModule.add_module_depend(['etk'])
	elif target.name=="Android":
		myModule.add_src_file('airtaudio/api/Android.cpp')
		# MacOsX core
		myModule.add_export_flag_CC(['-D__ANDROID_JAVA__'])
		myModule.add_module_depend(['ewol'])
	else:
		debug.warning("unknow target for AIRTAudio : " + target.name);
	
	myModule.add_export_path(tools.get_current_path(__file__))
	
	
	
	# add the currrent module at the 
	return myModule









