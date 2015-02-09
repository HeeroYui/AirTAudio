/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_CALLBACK_INFO_H__
#define __AIRTAUDIO_CALLBACK_INFO_H__

#include <thread>
#include <functional>
#include <airtaudio/base.h>

namespace airtaudio {
	// This global structure type is used to pass callback information
	// between the private RtAudio stream structure and global callback
	// handling functions.
	class CallbackInfo {
		public:
			std::thread* thread;
			airtaudio::AirTAudioCallback callback;
			void* apiInfo; // void pointer for API specific callback information
			bool isRunning;
			bool doRealtime;
			int32_t priority;
			
			// Default constructor.
			CallbackInfo() :
			  callback(nullptr),
			  apiInfo(nullptr),
			  isRunning(false),
			  doRealtime(false) {
				
			}
	};
};


#endif

