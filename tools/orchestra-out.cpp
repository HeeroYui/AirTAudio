/** @file
 * @author Edouard DUPIN 
 * @copyright 2015, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 */

#include <etk/etk.hpp>
#include <test-debug/debug.hpp>

#include <audio/orchestra/Interface.hpp>

int main(int _argc, const char **_argv) {
	// the only one init for etk:
	etk::init(_argc, _argv);
	for (int32_t iii=0; iii<_argc ; ++iii) {
		etk::String data = _argv[iii];
		if (    data == "-h"
		     || data == "--help") {
			std::cout << "Help : " << std::endl;
			std::cout << "    ./xxx ---" << std::endl;
			exit(0);
		}
	}
	audio::orchestra::Interface interface;
	TEST_PRINT("TODO : Need to write it");
	return 0;
}
