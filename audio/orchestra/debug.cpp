/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <audio/orchestra/debug.h>

int32_t audio::orchestra::getLogId() {
	static int32_t g_val = elog::registerInstance("audio-orchestra");
	return g_val;
}
