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
		'airtaudio/status.cpp',
		'airtaudio/type.cpp',
		'airtaudio/mode.cpp',
		'airtaudio/state.cpp',
		'airtaudio/error.cpp',
		'airtaudio/base.cpp',
		'airtaudio/Interface.cpp',
		'airtaudio/Flags.cpp',
		'airtaudio/Api.cpp',
		'airtaudio/DeviceInfo.cpp',
		'airtaudio/api/Dummy.cpp'
		])
	myModule.add_module_depend(['audio', 'etk'])
	# add all the time the dummy interface
	myModule.add_export_flag_CC(['-D__DUMMY__'])
	# TODO : Add a FILE interface:
	
	if target.name=="Windows":
		myModule.add_src_file([
			'airtaudio/api/Asio.cpp',
			'airtaudio/api/Ds.cpp',
			])
		# load optionnal API:
		myModule.add_optionnal_module_depend('asio', "__WINDOWS_ASIO__")
		myModule.add_optionnal_module_depend('ds', "__WINDOWS_DS__")
		myModule.add_optionnal_module_depend('wasapi', "__WINDOWS_WASAPI__")
	elif target.name=="Linux":
		myModule.add_src_file([
			'airtaudio/api/Alsa.cpp',
			'airtaudio/api/Jack.cpp',
			'airtaudio/api/Pulse.cpp',
			'airtaudio/api/Oss.cpp'
			])
		myModule.add_optionnal_module_depend('alsa', "__LINUX_ALSA__")
		#myModule.add_optionnal_module_depend('jack', "__UNIX_JACK__")
		#myModule.add_optionnal_module_depend('pulse', "__LINUX_PULSE__")
		#myModule.add_optionnal_module_depend('oss', "__LINUX_OSS__")
	elif target.name=="MacOs":
		myModule.add_src_file([
							   'airtaudio/api/Core.cpp',
							   'airtaudio/api/Oss.cpp'
							   ])
		# MacOsX core
		myModule.add_optionnal_module_depend('CoreAudio', "__MACOSX_CORE__")
		#myModule.add_export_flag_CC(['-D__MACOSX_CORE__'])
		#myModule.add_export_flag_LD("-framework CoreAudio")
	elif target.name=="IOs":
		myModule.add_src_file('airtaudio/api/CoreIos.mm')
		# IOsX core
		myModule.add_optionnal_module_depend('CoreAudio', "__IOS_CORE__")
		#myModule.add_export_flag_CC(['-D__IOS_CORE__'])
		#myModule.add_export_flag_LD("-framework CoreAudio")
		#myModule.add_export_flag_LD("-framework AudioToolbox")
	elif target.name=="Android":
		myModule.add_src_file('airtaudio/api/Android.cpp')
		# specidic java interface for android:
		myModule.add_optionnal_module_depend('ewolAndroidAudio', "__ANDROID_JAVA__")
		#myModule.add_export_flag_CC(['-D__ANDROID_JAVA__'])
		#myModule.add_module_depend(['ewol'])
	else:
		debug.warning("unknow target for AIRTAudio : " + target.name);
	
	myModule.add_export_path(tools.get_current_path(__file__))
	
	# add the currrent module at the 
	return myModule


