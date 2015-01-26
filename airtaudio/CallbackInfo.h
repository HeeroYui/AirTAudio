/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
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
			void* object; // Used as a "this" pointer.
			std::thread* thread;
			airtaudio::AirTAudioCallback callback;
			void* apiInfo; // void pointer for API specific callback information
			bool isRunning;
			bool doRealtime;
			int32_t priority;
			
			// Default constructor.
			CallbackInfo() :
			  object(nullptr),
			  callback(nullptr),
			  apiInfo(nullptr),
			  isRunning(false),
			  doRealtime(false) {
				
			}
	};
};


#endif

