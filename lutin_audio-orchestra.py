#!/usr/bin/python
import lutin.module as module
import lutin.tools as tools
import lutin.debug as debug

def get_desc():
	return "Generic wrapper on all audio interface"


def create(target):
	myModule = module.Module(__file__, 'audio-orchestra', 'LIBRARY')
	
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
	myModule.add_export_flag('c++', ['-DORCHESTRA_BUILD_DUMMY'])
	# TODO : Add a FILE interface:
	
	if target.name=="Windows":
		myModule.add_src_file([
			'audio/orchestra/api/Asio.cpp',
			'audio/orchestra/api/Ds.cpp',
			])
		# load optionnal API:
		myModule.add_optionnal_module_depend('asio', ["c++", "-DORCHESTRA_BUILD_ASIO"])
		myModule.add_optionnal_module_depend('ds', ["c++", "-DORCHESTRA_BUILD_DS"])
		myModule.add_optionnal_module_depend('wasapi', ["c++", "-DORCHESTRA_BUILD_WASAPI"])
	elif target.name=="Linux":
		myModule.add_src_file([
			'audio/orchestra/api/Alsa.cpp',
			'audio/orchestra/api/Jack.cpp',
			'audio/orchestra/api/Pulse.cpp',
			'audio/orchestra/api/PulseDeviceList.cpp',
			'audio/orchestra/api/Oss.cpp'
			])
		myModule.add_optionnal_module_depend('alsa', ["c++", "-DORCHESTRA_BUILD_ALSA"])
		myModule.add_optionnal_module_depend('jack', ["c++", "-DORCHESTRA_BUILD_JACK"])
		myModule.add_optionnal_module_depend('pulse', ["c++", "-DORCHESTRA_BUILD_PULSE"])
		myModule.add_optionnal_module_depend('oss', ["c++", "-DORCHESTRA_BUILD_OSS"])
	elif target.name=="MacOs":
		myModule.add_src_file([
							   'audio/orchestra/api/Core.cpp',
							   'audio/orchestra/api/Oss.cpp'
							   ])
		# MacOsX core
		myModule.add_optionnal_module_depend('CoreAudio', ["c++", "-DORCHESTRA_BUILD_MACOSX_CORE"])
	elif target.name=="IOs":
		myModule.add_src_file('audio/orchestra/api/CoreIos.mm')
		# IOsX core
		myModule.add_optionnal_module_depend('CoreAudio', ["c++", "-DORCHESTRA_BUILD_IOS_CORE"])
	elif target.name=="Android":
		myModule.add_src_file('android/org/musicdsp/orchestra/Constants.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/ManagerCallback.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/Orchestra.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/InterfaceInput.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/InterfaceOutput.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/Manager.java')
		# create inter language interface
		myModule.add_src_file('org.musicdsp.orchestra.Constants.javah')
		myModule.add_path(tools.get_current_path(__file__) + '/android/', type='java')
		myModule.add_module_depend(['SDK', 'jvm-basics', 'ejson'])
		myModule.add_export_flag('c++', ['-DORCHESTRA_BUILD_JAVA'])
		
		myModule.add_src_file('audio/orchestra/api/Android.cpp')
		myModule.add_src_file('audio/orchestra/api/AndroidNativeInterface.cpp')
		# add tre creator of the basic java class ...
		target.add_action("PACKAGE", 11, "audio-orchestra-out-wrapper", tool_generate_add_java_section_in_class)
	else:
		debug.warning("unknow target for audio_orchestra : " + target.name);
	
	myModule.add_export_path(tools.get_current_path(__file__))
	
	return myModule



##################################################################
##
## Android specific section
##
##################################################################
def tool_generate_add_java_section_in_class(target, module, package_name):
	module.pkg_add("GENERATE_SECTION__IMPORT", [
		"import org.musicdsp.orchestra.Manager;"
		])
	module.pkg_add("GENERATE_SECTION__DECLARE", [
		"private Manager MANAGER;"
		])
	module.pkg_add("GENERATE_SECTION__CONSTRUCTOR", [
		"// load audio maneger if it does not work, it is not critical ...",
		"try {",
		"	MANAGER = new Manager();",
		"} catch (RuntimeException e) {",
		"	Log.e(\"" + package_name + "\", \"Can not load Audio interface (maybe not really needed) :\" + e);",
		"}"
		])
	
	




