/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */
#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <audio/orchestra/api/PulseDeviceList.h>
#include <audio/orchestra/debug.h>

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
static void callbackStateMachine(pa_context* _contex, void *_userdata) {
	pa_context_state_t state;
	int *pulseAudioReady = static_cast<int*>(_userdata);
	state = pa_context_get_state(_contex);
	switch  (state) {
		// There are just here for reference
		case PA_CONTEXT_UNCONNECTED:
			ATA_INFO("pulse state: PA_CONTEXT_UNCONNECTED");
			break;
		case PA_CONTEXT_CONNECTING:
			ATA_INFO("pulse state: PA_CONTEXT_CONNECTING");
			break;
		case PA_CONTEXT_AUTHORIZING:
			ATA_INFO("pulse state: PA_CONTEXT_AUTHORIZING");
			break;
		case PA_CONTEXT_SETTING_NAME:
			ATA_INFO("pulse state: PA_CONTEXT_SETTING_NAME");
			break;
		default:
			ATA_INFO("pulse state: default");
			break;
		case PA_CONTEXT_FAILED:
			*pulseAudioReady = 2;
			ATA_INFO("pulse state: PA_CONTEXT_FAILED");
			break;
		case PA_CONTEXT_TERMINATED:
			*pulseAudioReady = 2;
			ATA_INFO("pulse state: PA_CONTEXT_TERMINATED");
			break;
		case PA_CONTEXT_READY:
			*pulseAudioReady = 1;
			ATA_INFO("pulse state: PA_CONTEXT_READY");
			break;
	}
}
// Callback on getting data from pulseaudio:
static void callbackGetSinkList(pa_context* _contex, const pa_sink_info* _info, int _eol, void* _userdata) {
	std::vector<audio::orchestra::api::pulse::Element>* list = static_cast<std::vector<audio::orchestra::api::pulse::Element>*>(_userdata);
	// If eol is set to a positive number, you're at the end of the list
	if (_eol > 0) {
		return;
	}
	ATA_INFO("find output : " << _info->name);
	list->push_back(audio::orchestra::api::pulse::Element(_info->index, false, _info->name, _info->description));
}

// allback to get data from pulseaudio:
static void callbackGetSourceList(pa_context* _contex, const pa_source_info* _info, int _eol, void* _userdata) {
	std::vector<audio::orchestra::api::pulse::Element>* list = static_cast<std::vector<audio::orchestra::api::pulse::Element>*>(_userdata);
	if (_eol > 0) {
		return;
	}
	ATA_INFO("find input : " << _info->name);
	list->push_back(audio::orchestra::api::pulse::Element(_info->index, true, _info->name, _info->description));
}

std::vector<audio::orchestra::api::pulse::Element> audio::orchestra::api::pulse::getDeviceList() {
	// Define our pulse audio loop and connection variables
	pa_mainloop* pulseAudioMainLoop;
	pa_mainloop_api* pulseAudioMainLoopAPI;
	pa_operation* pulseAudioOperation;
	pa_context* pulseAudioContex;
	pa_context_flags_t pulseAudioFlags;
	std::vector<audio::orchestra::api::pulse::Element> out;
	// We'll need these state variables to keep track of our requests
	int state = 0;
	int pulseAudioReady = 0;
	// Create a mainloop API and connection to the default server
	pulseAudioMainLoop = pa_mainloop_new();
	pulseAudioMainLoopAPI = pa_mainloop_get_api(pulseAudioMainLoop);
	pulseAudioContex = pa_context_new(pulseAudioMainLoopAPI, "test");
	// This function connects to the pulse server
	pa_context_connect(pulseAudioContex, NULL, pulseAudioFlags, NULL);
	// If there's an error, the callback will set pulseAudioReady
	pa_context_set_state_callback(pulseAudioContex, callbackStateMachine, &pulseAudioReady);
	ATA_INFO("start main loop...");
	while (true) {
		ATA_INFO("loop");
		// We can't do anything until PA is ready, so just iterate the mainloop
		// and continue
		if (pulseAudioReady == 0) {
			ATA_INFO("Pulse not ready");
			pa_mainloop_iterate(pulseAudioMainLoop, 1, nullptr);
			continue;
		}
		// We couldn't get a connection to the server, so exit out
		if (pulseAudioReady == 2) {
			ATA_INFO("pulse not ready");
			pa_context_disconnect(pulseAudioContex);
			pa_context_unref(pulseAudioContex);
			pa_mainloop_free(pulseAudioMainLoop);
			ATA_ERROR("Pulse interface error: Can not connect to the pulseaudio iterface...");
			return out;
		}
		// At this point, we're connected to the server and ready to make
		// requests
		switch (state) {
			// State 0: we haven't done anything yet
			case 0:
				ATA_INFO("Request sink list");
				pulseAudioOperation = pa_context_get_sink_info_list(pulseAudioContex,
				                                                    callbackGetSinkList,
				                                                    &out);
				state++;
				break;
			case 1:
				// Now we wait for our operation to complete.  When it's
				// complete our pa_output_devicelist is filled out, and we move
				// along to the next state
				if (pa_operation_get_state(pulseAudioOperation) == PA_OPERATION_DONE) {
					pa_operation_unref(pulseAudioOperation);
					ATA_INFO("Request sources list");
					pulseAudioOperation = pa_context_get_source_info_list(pulseAudioContex,
					                                                      callbackGetSourceList,
					                                                      &out);
					state++;
				}
				break;
			case 2:
				if (pa_operation_get_state(pulseAudioOperation) == PA_OPERATION_DONE) {
					ATA_INFO("All is done");
					// Now we're done, clean up and disconnect and return
					pa_operation_unref(pulseAudioOperation);
					pa_context_disconnect(pulseAudioContex);
					pa_context_unref(pulseAudioContex);
					pa_mainloop_free(pulseAudioMainLoop);
					return out;
				}
			break;
			default:
				// We should never see this state
				ATA_ERROR("Error in getting the devices list ...");
				return out;
		}
		// Iterate the main loop ..
		pa_mainloop_iterate(pulseAudioMainLoop, 1, nullptr);
	}
	return out;
}
