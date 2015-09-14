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
	myModule.add_header_file([
		'audio/orchestra/debug.h',
		'audio/orchestra/status.h',
		'audio/orchestra/type.h',
		'audio/orchestra/mode.h',
		'audio/orchestra/state.h',
		'audio/orchestra/error.h',
		'audio/orchestra/base.h',
		'audio/orchestra/Interface.h',
		'audio/orchestra/Flags.h',
		'audio/orchestra/Api.h',
		'audio/orchestra/DeviceInfo.h',
		'audio/orchestra/StreamOptions.h',
		'audio/orchestra/CallbackInfo.h',
		'audio/orchestra/StreamParameters.h'
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
			'audio/orchestra/api/PulseDeviceList.cpp'
			])
		myModule.add_optionnal_module_depend('alsa', ["c++", "-DORCHESTRA_BUILD_ALSA"])
		myModule.add_optionnal_module_depend('jack', ["c++", "-DORCHESTRA_BUILD_JACK"])
		myModule.add_optionnal_module_depend('pulse', ["c++", "-DORCHESTRA_BUILD_PULSE"])
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
		myModule.add_src_file('android/org/musicdsp/orchestra/OrchestraConstants.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/OrchestraManagerCallback.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/OrchestraNative.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/OrchestraInterfaceInput.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/OrchestraInterfaceOutput.java')
		myModule.add_src_file('android/org/musicdsp/orchestra/OrchestraManager.java')
		# create inter language interface
		myModule.add_src_file('org.musicdsp.orchestra.OrchestraConstants.javah')
		myModule.add_path(tools.get_current_path(__file__) + '/android/', type='java')
		myModule.add_module_depend(['SDK', 'jvm-basics', 'ejson'])
		myModule.add_export_flag('c++', ['-DORCHESTRA_BUILD_JAVA'])
		
		myModule.add_src_file('audio/orchestra/api/Android.cpp')
		myModule.add_src_file('audio/orchestra/api/AndroidNativeInterface.cpp')
		# add tre creator of the basic java class ...
		target.add_action("PACKAGE", 11, "audio-orchestra-out-wrapper", tool_generate_add_java_section_in_class)
	else:
		debug.warning("unknow target for audio_orchestra : " + target.name);
	
	myModule.add_path(tools.get_current_path(__file__))
	
	return myModule



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
	




