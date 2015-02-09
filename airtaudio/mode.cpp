/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#include <airtaudio/mode.h>
#include <airtaudio/debug.h>

int32_t airtaudio::modeToIdTable(enum mode _mode) {
	switch (_mode) {
		case mode_unknow:
		case mode_duplex:
		case mode_output:
			return 0;
		case mode_input:
			return 1;
	}
	return 0;
}