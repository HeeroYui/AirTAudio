#!/usr/bin/python
import lutin.module as module
import lutin.tools as tools
import lutin.debug as debug


def get_type():
	return "LIBRARY"

def get_desc():
	return "Generic wrapper on all audio interface"

def get_licence():
	return "APACHE-2"

def get_compagny_type():
	return "com"

def get_compagny_name():
	return "atria-soft"

def get_maintainer():
	return "authors.txt"

def get_version():
	return "version.txt"

def create(target, module_name):
	my_module = module.Module(__file__, module_name, get_type())
	my_module.add_src_file([
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
	my_module.add_header_file([
		'audio/orchestra/debug.hpp',
		'audio/orchestra/status.hpp',
		'audio/orchestra/type.hpp',
		'audio/orchestra/mode.hpp',
		'audio/orchestra/state.hpp',
		'audio/orchestra/error.hpp',
		'audio/orchestra/base.hpp',
		'audio/orchestra/Interface.hpp',
		'audio/orchestra/Flags.hpp',
		'audio/orchestra/Api.hpp',
		'audio/orchestra/DeviceInfo.hpp',
		'audio/orchestra/StreamOptions.hpp',
		'audio/orchestra/CallbackInfo.hpp',
		'audio/orchestra/StreamParameters.hpp'
		])
	my_module.add_depend([
	    'audio',
	    'etk'
	    ])
	# add all the time the dummy interface
	my_module.add_flag('c++', ['-DORCHESTRA_BUILD_DUMMY'], export=True)
	# TODO : Add a FILE interface:
	
	if "Windows" in target.get_type():
		my_module.add_src_file([
			'audio/orchestra/api/Asio.cpp',
			'audio/orchestra/api/Ds.cpp',
			])
		# load optionnal API:
		my_module.add_optionnal_depend('asio', ["c++", "-DORCHESTRA_BUILD_ASIO"])
		my_module.add_optionnal_depend('ds', ["c++", "-DORCHESTRA_BUILD_DS"])
		my_module.add_optionnal_depend('wasapi', ["c++", "-DORCHESTRA_BUILD_WASAPI"])
	elif "Linux" in target.get_type():
		my_module.add_src_file([
			'audio/orchestra/api/Alsa.cpp',
			'audio/orchestra/api/Jack.cpp',
			'audio/orchestra/api/Pulse.cpp',
			'audio/orchestra/api/PulseDeviceList.cpp'
			])
		my_module.add_optionnal_depend('alsa', ["c++", "-DORCHESTRA_BUILD_ALSA"])
		my_module.add_optionnal_depend('jack', ["c++", "-DORCHESTRA_BUILD_JACK"])
		my_module.add_optionnal_depend('pulse', ["c++", "-DORCHESTRA_BUILD_PULSE"])
	elif "MacOs" in target.get_type():
		my_module.add_src_file([
							   'audio/orchestra/api/Core.cpp'
							   ])
		# MacOsX core
		my_module.add_optionnal_depend('CoreAudio', ["c++", "-DORCHESTRA_BUILD_MACOSX_CORE"])
	elif "IOs" in target.get_type():
		my_module.add_src_file('audio/orchestra/api/CoreIos.mm')
		# IOsX core
		my_module.add_optionnal_depend('CoreAudio', ["c++", "-DORCHESTRA_BUILD_IOS_CORE"])
	elif "Android" in target.get_type():
		my_module.add_src_file('android/org/musicdsp/orchestra/OrchestraConstants.java')
		my_module.add_src_file('android/org/musicdsp/orchestra/OrchestraManagerCallback.java')
		my_module.add_src_file('android/org/musicdsp/orchestra/OrchestraNative.java')
		my_module.add_src_file('android/org/musicdsp/orchestra/OrchestraInterfaceInput.java')
		my_module.add_src_file('android/org/musicdsp/orchestra/OrchestraInterfaceOutput.java')
		my_module.add_src_file('android/org/musicdsp/orchestra/OrchestraManager.java')
		# create inter language interfacef
		my_module.add_src_file('org.musicdsp.orchestra.OrchestraConstants.javah')
		my_module.add_path(tools.get_current_path(__file__) + '/android/', type='java')
		my_module.add_depend(['SDK', 'jvm-basics', 'ejson'])
		my_module.add_flag('c++', ['-DORCHESTRA_BUILD_JAVA'], export=True)
		
		my_module.add_src_file('audio/orchestra/api/Android.cpp')
		my_module.add_src_file('audio/orchestra/api/AndroidNativeInterface.cpp')
		# add tre creator of the basic java class ...
		target.add_action("BINARY", 11, "audio-orchestra-out-wrapper", tool_generate_add_java_section_in_class)
	else:
		debug.warning("unknow target for audio_orchestra : " + target.name);
	
	my_module.add_path(tools.get_current_path(__file__))
	
	return my_module



##################################################################
##
## Android specific section
##
##################################################################
def tool_generate_add_java_section_in_class(target, module, package_name):
	module.pkg_add("GENERATE_SECTION__IMPORT", [
		"import org.musicdsp.orchestra.OrchestraManager;"
		])
	module.pkg_add("GENERATE_SECTION__DECLARE", [
		"private OrchestraManager m_audioManagerHandle;"
		])
	module.pkg_add("GENERATE_SECTION__CONSTRUCTOR", [
		"// load audio maneger if it does not work, it is not critical ...",
		"try {",
		"	m_audioManagerHandle = new OrchestraManager();",
		"} catch (RuntimeException e) {",
		"	Log.e(\"" + package_name + "\", \"Can not load Audio interface (maybe not really needed) :\" + e);",
		"}"
		])
	module.pkg_add("GENERATE_SECTION__ON_CREATE", [
		"if (m_audioManagerHandle != null) {",
		"	m_audioManagerHandle.onCreate();",
		"}"
		])
	module.pkg_add("GENERATE_SECTION__ON_START", [
		"if (m_audioManagerHandle != null) {",
		"	m_audioManagerHandle.onStart();",
		"}"
		])
	module.pkg_add("GENERATE_SECTION__ON_RESTART", [
		"if (m_audioManagerHandle != null) {",
		"	m_audioManagerHandle.onRestart();",
		"}"
		])
	module.pkg_add("GENERATE_SECTION__ON_RESUME", [
		"if (m_audioManagerHandle != null) {",
		"	m_audioManagerHandle.onResume();",
		"}"
		])
	module.pkg_add("GENERATE_SECTION__ON_PAUSE", [
		"if (m_audioManagerHandle != null) {",
		"	m_audioManagerHandle.onPause();",
		"}"
		])
	module.pkg_add("GENERATE_SECTION__ON_STOP", [
		"if (m_audioManagerHandle != null) {",
		"	m_audioManagerHandle.onStop();",
		"}"
		])
	module.pkg_add("GENERATE_SECTION__ON_DESTROY", [
		"// Destroy the AdView.",
		"if (m_audioManagerHandle != null) {",
		"	m_audioManagerHandle.onDestroy();",
		"}"
		])
	




