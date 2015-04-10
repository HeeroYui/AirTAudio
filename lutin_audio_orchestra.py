#!/usr/bin/python
import lutinModule as module
import lutinTools as tools
import lutinDebug as debug

def get_desc():
	return "audio_orchestra : Generic wrapper on all audio interface"


def create(target):
	myModule = module.Module(__file__, 'audio_orchestra', 'LIBRARY')
	
	myModule.add_src_file([
		'audio/orchestra/debug.cpp',
		'audio/orchestra/status.cpp',
		'audio/orchestra/type.cpp',
		'audio/orchestra/mode.cpp',
		'audio/orchestra/state.cpp',
		'audio/orchestra/error.cpp',
		'audio/orchestra/base.cpp',
		'audio/orchestra/Interface.cpp',
		'audio/orchestra/Flags.cpp',
		'audio/orchestra/Api.cpp',
		'audio/orchestra/DeviceInfo.cpp',
		'audio/orchestra/StreamOptions.cpp',
		'audio/orchestra/api/Dummy.cpp'
		])
	myModule.add_module_depend(['audio', 'etk'])
	# add all the time the dummy interface
	myModule.add_export_flag_CC(['-DORCHESTRA_BUILD_DUMMY'])
	# TODO : Add a FILE interface:
	
	if target.name=="Windows":
		myModule.add_src_file([
			'audio/orchestra/api/Asio.cpp',
			'audio/orchestra/api/Ds.cpp',
			])
		# load optionnal API:
		myModule.add_optionnal_module_depend('asio', "ORCHESTRA_BUILD_ASIO")
		myModule.add_optionnal_module_depend('ds', "ORCHESTRA_BUILD_DS")
		myModule.add_optionnal_module_depend('wasapi', "ORCHESTRA_BUILD_WASAPI")
	elif target.name=="Linux":
		myModule.add_src_file([
			'audio/orchestra/api/Alsa.cpp',
			'audio/orchestra/api/Jack.cpp',
			'audio/orchestra/api/Pulse.cpp',
			'audio/orchestra/api/Oss.cpp'
			])
		myModule.add_optionnal_module_depend('alsa', "ORCHESTRA_BUILD_ALSA")
		myModule.add_optionnal_module_depend('jack', "ORCHESTRA_BUILD_JACK")
		myModule.add_optionnal_module_depend('pulse', "ORCHESTRA_BUILD_PULSE")
		myModule.add_optionnal_module_depend('oss', "ORCHESTRA_BUILD_OSS")
	elif target.name=="MacOs":
		myModule.add_src_file([
							   'audio/orchestra/api/Core.cpp',
							   'audio/orchestra/api/Oss.cpp'
							   ])
		# MacOsX core
		myModule.add_optionnal_module_depend('CoreAudio', "ORCHESTRA_BUILD_MACOSX_CORE")
	elif target.name=="IOs":
		myModule.add_src_file('audio/orchestra/api/CoreIos.mm')
		# IOsX core
		myModule.add_optionnal_module_depend('CoreAudio', "ORCHESTRA_BUILD_IOS_CORE")
	elif target.name=="Android":
		myModule.add_src_file('audio/orchestra/api/Android.cpp')
		# specidic java interface for android:
		myModule.add_optionnal_module_depend('ewolAndroidAudio', "ORCHESTRA_BUILD_JAVA")
		#myModule.add_module_depend(['ewol'])
	else:
		debug.warning("unknow target for audio_orchestra : " + target.name);
	
	myModule.add_export_path(tools.get_current_path(__file__))
	
	# add the currrent module at the 
	return myModule


