/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <audio/orchestra/type.h>
#include <audio/orchestra/debug.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <climits>

#undef __class__
#define __class__ "type"

const std::string audio::orchestra::type_undefined = "undefined";
const std::string audio::orchestra::type_alsa = "alsa";
const std::string audio::orchestra::type_pulse = "pulse";
const std::string audio::orchestra::type_oss = "oss";
const std::string audio::orchestra::type_jack = "jack";
const std::string audio::orchestra::type_coreOSX = "coreOSX";
const std::string audio::orchestra::type_coreIOS = "coreIOS";
const std::string audio::orchestra::type_asio = "asio";
const std::string audio::orchestra::type_ds = "ds";
const std::string audio::orchestra::type_java = "java";
const std::string audio::orchestra::type_dummy = "dummy";
