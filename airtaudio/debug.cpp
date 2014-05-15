/**
 * @author Edouard DUPIN
 * 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * 
 * @license BSD 3 clauses (see license file)
 */

#include <airtaudio/debug.h>

int32_t airtaudio::getLogId() {
	static int32_t g_val = etk::log::registerInstance("airtaudio");
	return g_val;
}
