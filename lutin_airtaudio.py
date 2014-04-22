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
		'airtaudio/api/Alsa.cpp',
		'airtaudio/api/Android.cpp',
		'airtaudio/api/Asio.cpp',
		'airtaudio/api/Core.cpp',
		'airtaudio/api/Ds.cpp',
		'airtaudio/api/Dummy.cpp',
		'airtaudio/api/Jack.cpp',
		'airtaudio/api/Oss.cpp',
		'airtaudio/api/Pulse.cpp'
		])
	
	myModule.add_export_flag_CC(['-D__AIRTAUDIO_API_DUMMY_H__'])
	if target.name=="Windows":
		# ASIO API on Windows
		myModule.add_export_flag_CC(['__WINDOWS_ASIO__'])
		# Windows DirectSound API
		#myModule.add_export_flag_CC(['__WINDOWS_DS__'])
	elif target.name=="Linux":
		# Linux Alsa API
		#myModule.add_export_flag_CC(['-D__LINUX_ALSA__'])
		#myModule.add_export_flag_LD("-lasound")
		# Linux Jack API
		#myModule.add_export_flag_CC(['-D__UNIX_JACK__'])
		#myModule.add_export_flag_LD("-ljack")
		# Linux PulseAudio API
		myModule.add_export_flag_CC(['-D__LINUX_PULSE__'])
		myModule.add_export_flag_LD("-lpulse-simple")
		myModule.add_export_flag_LD("-lpulse")
	elif target.name=="MacOs":
		# MacOsX core
		myModule.add_export_flag_CC(['-D__MACOSX_CORE__'])
		myModule.add_export_flag_LD("-framework CoreAudio")
	elif target.name=="IOs":
		# IOsX core
		#myModule.add_export_flag_CC(['-D__IOS_CORE__'])
		#myModule.add_export_flag_LD("-framework CoreAudio")
		pass
	elif target.name=="Android":
		# MacOsX core
		myModule.add_export_flag_CC(['-D__ANDROID_JAVA__'])
	else:
		debug.warning("unknow target for RTAudio : " + target.name);
	
	myModule.add_export_path(tools.get_current_path(__file__))
	myModule.add_path(tools.get_current_path(__file__)+"/rtaudio/")
	myModule.add_path(tools.get_current_path(__file__)+"/rtaudio/include/")
	
	# TODO : Remove this when debug will be unified ...
	# name of the dependency
	myModule.add_module_depend(['ewol'])
	
	# add the currrent module at the 
	return myModule









