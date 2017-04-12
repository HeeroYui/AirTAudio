/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if defined(ORCHESTRA_BUILD_PULSE)

#include <cstdio>
#include <cstring>
#include <pulse/pulseaudio.h>
#include <audio/orchestra/api/PulseDeviceList.hpp>
#include <audio/orchestra/debug.hpp>
#include <audio/Time.hpp>
#include <audio/Duration.hpp>
#include <audio/format.hpp>
#include <etk/stdTools.hpp>

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
static void callbackStateMachine(pa_context* _contex, void *_userdata) {
	pa_context_state_t state;
	int *pulseAudioReady = static_cast<int*>(_userdata);
	state = pa_context_get_state(_contex);
	switch  (state) {
		// There are just here for reference
		case PA_CONTEXT_UNCONNECTED:
			ATA_VERBOSE("pulse state: PA_CONTEXT_UNCONNECTED");
			break;
		case PA_CONTEXT_CONNECTING:
			ATA_VERBOSE("pulse state: PA_CONTEXT_CONNECTING");
			break;
		case PA_CONTEXT_AUTHORIZING:
			ATA_VERBOSE("pulse state: PA_CONTEXT_AUTHORIZING");
			break;
		case PA_CONTEXT_SETTING_NAME:
			ATA_VERBOSE("pulse state: PA_CONTEXT_SETTING_NAME");
			break;
		default:
			ATA_VERBOSE("pulse state: default");
			break;
		case PA_CONTEXT_FAILED:
			*pulseAudioReady = 2;
			ATA_VERBOSE("pulse state: PA_CONTEXT_FAILED");
			break;
		case PA_CONTEXT_TERMINATED:
			*pulseAudioReady = 2;
			ATA_VERBOSE("pulse state: PA_CONTEXT_TERMINATED");
			break;
		case PA_CONTEXT_READY:
			*pulseAudioReady = 1;
			ATA_VERBOSE("pulse state: PA_CONTEXT_READY");
			break;
	}
}

static audio::format getFormatFromPulseFormat(enum pa_sample_format _format) {
	switch (_format) {
		case PA_SAMPLE_U8:
			return audio::format_int8;
			break;
		case PA_SAMPLE_ALAW:
			ATA_ERROR("Not supported: uint8_t a-law");
			return audio::format_unknow;
		case PA_SAMPLE_ULAW:
			ATA_ERROR("Not supported: uint8_t mu-law");
			return audio::format_unknow;
		case PA_SAMPLE_S16LE:
			return audio::format_int16;
			break;
		case PA_SAMPLE_S16BE:
			return audio::format_int16;
			break;
		case PA_SAMPLE_FLOAT32LE:
			return audio::format_float;
			break;
		case PA_SAMPLE_FLOAT32BE:
			return audio::format_float;
			break;
		case PA_SAMPLE_S32LE:
			return audio::format_int32;
			break;
		case PA_SAMPLE_S32BE:
			return audio::format_int32;
			break;
		case PA_SAMPLE_S24LE:
			return audio::format_int24;
			break;
		case PA_SAMPLE_S24BE:
			return audio::format_int24;
			break;
		case PA_SAMPLE_S24_32LE:
			return audio::format_int24_on_int32;
			break;
		case PA_SAMPLE_S24_32BE:
			return audio::format_int24_on_int32;
			break;
		case PA_SAMPLE_INVALID:
		case PA_SAMPLE_MAX:
			ATA_ERROR("Not supported: invalid");
			return audio::format_unknow;
	}
	ATA_ERROR("Not supported: UNKNOW flag...");
	return audio::format_unknow;
}

static std::vector<audio::channel> getChannelOrderFromPulseChannel(const struct pa_channel_map& _map) {
	std::vector<audio::channel> out;
	
	for (int32_t iii=0; iii<_map.channels; ++iii) {
		switch(_map.map[iii]) {
			default:
			case PA_CHANNEL_POSITION_MAX:
			case PA_CHANNEL_POSITION_INVALID:
				out.push_back(audio::channel_unknow);
				break;
			case PA_CHANNEL_POSITION_MONO:
			case PA_CHANNEL_POSITION_FRONT_CENTER:
				out.push_back(audio::channel_frontCenter);
				break;
			case PA_CHANNEL_POSITION_FRONT_LEFT:
				out.push_back(audio::channel_frontLeft);
				break;
			case PA_CHANNEL_POSITION_FRONT_RIGHT:
				out.push_back(audio::channel_frontRight);
				break;
			case PA_CHANNEL_POSITION_REAR_CENTER:
				out.push_back(audio::channel_rearCenter);
				break;
			case PA_CHANNEL_POSITION_REAR_LEFT:
				out.push_back(audio::channel_rearLeft);
				break;
			case PA_CHANNEL_POSITION_REAR_RIGHT:
				out.push_back(audio::channel_rearRight);
				break;
			case PA_CHANNEL_POSITION_LFE:
				out.push_back(audio::channel_lfe);
				break;
			case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
				out.push_back(audio::channel_centerLeft);
				break;
			case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
				out.push_back(audio::channel_centerRight);
				break;
			case PA_CHANNEL_POSITION_SIDE_LEFT:
				out.push_back(audio::channel_topCenterLeft);
				break;
			case PA_CHANNEL_POSITION_SIDE_RIGHT:
				out.push_back(audio::channel_topCenterRight);
				break;
			case PA_CHANNEL_POSITION_TOP_CENTER:
			case PA_CHANNEL_POSITION_TOP_FRONT_CENTER:
				out.push_back(audio::channel_topFrontCenter);
				break;
			case PA_CHANNEL_POSITION_TOP_FRONT_LEFT:
				out.push_back(audio::channel_topFrontLeft);
				break;
			case PA_CHANNEL_POSITION_TOP_FRONT_RIGHT:
				out.push_back(audio::channel_topFrontRight);
				break;
			case PA_CHANNEL_POSITION_TOP_REAR_LEFT:
				out.push_back(audio::channel_topRearLeft);
				break;
			case PA_CHANNEL_POSITION_TOP_REAR_RIGHT:
				out.push_back(audio::channel_topRearRight);
				break;
			case PA_CHANNEL_POSITION_TOP_REAR_CENTER:
				out.push_back(audio::channel_topRearCenter);
				break;
			case PA_CHANNEL_POSITION_AUX0:  out.push_back(audio::channel_aux0);  break;
			case PA_CHANNEL_POSITION_AUX1:  out.push_back(audio::channel_aux1);  break;
			case PA_CHANNEL_POSITION_AUX2:  out.push_back(audio::channel_aux2);  break;
			case PA_CHANNEL_POSITION_AUX3:  out.push_back(audio::channel_aux3);  break;
			case PA_CHANNEL_POSITION_AUX4:  out.push_back(audio::channel_aux4);  break;
			case PA_CHANNEL_POSITION_AUX5:  out.push_back(audio::channel_aux5);  break;
			case PA_CHANNEL_POSITION_AUX6:  out.push_back(audio::channel_aux6);  break;
			case PA_CHANNEL_POSITION_AUX7:  out.push_back(audio::channel_aux7);  break;
			case PA_CHANNEL_POSITION_AUX8:  out.push_back(audio::channel_aux8);  break;
			case PA_CHANNEL_POSITION_AUX9:  out.push_back(audio::channel_aux9);  break;
			case PA_CHANNEL_POSITION_AUX10: out.push_back(audio::channel_aux10); break;
			case PA_CHANNEL_POSITION_AUX11: out.push_back(audio::channel_aux11); break;
			case PA_CHANNEL_POSITION_AUX12: out.push_back(audio::channel_aux12); break;
			case PA_CHANNEL_POSITION_AUX13: out.push_back(audio::channel_aux13); break;
			case PA_CHANNEL_POSITION_AUX14: out.push_back(audio::channel_aux14); break;
			case PA_CHANNEL_POSITION_AUX15: out.push_back(audio::channel_aux15); break;
			case PA_CHANNEL_POSITION_AUX16: out.push_back(audio::channel_aux16); break;
			case PA_CHANNEL_POSITION_AUX17: out.push_back(audio::channel_aux17); break;
			case PA_CHANNEL_POSITION_AUX18: out.push_back(audio::channel_aux18); break;
			case PA_CHANNEL_POSITION_AUX19: out.push_back(audio::channel_aux19); break;
			case PA_CHANNEL_POSITION_AUX20: out.push_back(audio::channel_aux20); break;
			case PA_CHANNEL_POSITION_AUX21: out.push_back(audio::channel_aux21); break;
			case PA_CHANNEL_POSITION_AUX22: out.push_back(audio::channel_aux22); break;
			case PA_CHANNEL_POSITION_AUX23: out.push_back(audio::channel_aux23); break;
			case PA_CHANNEL_POSITION_AUX24: out.push_back(audio::channel_aux24); break;
			case PA_CHANNEL_POSITION_AUX25: out.push_back(audio::channel_aux25); break;
			case PA_CHANNEL_POSITION_AUX26: out.push_back(audio::channel_aux26); break;
			case PA_CHANNEL_POSITION_AUX27: out.push_back(audio::channel_aux27); break;
			case PA_CHANNEL_POSITION_AUX28: out.push_back(audio::channel_aux28); break;
			case PA_CHANNEL_POSITION_AUX29: out.push_back(audio::channel_aux29); break;
			case PA_CHANNEL_POSITION_AUX30: out.push_back(audio::channel_aux30); break;
			case PA_CHANNEL_POSITION_AUX31: out.push_back(audio::channel_aux31); break;
		}
	}
	
	return out;
}
// Callback on getting data from pulseaudio:
static void callbackGetSinkList(pa_context* _contex, const pa_sink_info* _info, int _eol, void* _userdata) {
	std::vector<audio::orchestra::DeviceInfo>* list = static_cast<std::vector<audio::orchestra::DeviceInfo>*>(_userdata);
	// If eol is set to a positive number, you're at the end of the list
	if (_eol > 0) {
		return;
	}
	audio::orchestra::DeviceInfo info;
	info.isCorrect = true;
	info.input = false;
	info.name = _info->name;
	info.desc = _info->description;
	info.sampleRates.push_back(_info->sample_spec.rate);
	info.nativeFormats.push_back(getFormatFromPulseFormat(_info->sample_spec.format));
	info.channels = getChannelOrderFromPulseChannel(_info->channel_map);
	ATA_VERBOSE("plop=" << _info->index << " " << _info->name);
	//ATA_DEBUG("          ports=" << _info->n_ports);
	list->push_back(info);
}

// allback to get data from pulseaudio:
static void callbackGetSourceList(pa_context* _contex, const pa_source_info* _info, int _eol, void* _userdata) {
	std::vector<audio::orchestra::DeviceInfo>* list = static_cast<std::vector<audio::orchestra::DeviceInfo>*>(_userdata);
	if (_eol > 0) {
		return;
	}
	audio::orchestra::DeviceInfo info;
	info.isCorrect = true;
	info.input = true;
	info.name = _info->name;
	info.desc = _info->description;
	info.sampleRates.push_back(_info->sample_spec.rate);
	info.nativeFormats.push_back(getFormatFromPulseFormat(_info->sample_spec.format));
	info.channels = getChannelOrderFromPulseChannel(_info->channel_map);
	ATA_VERBOSE("plop=" << _info->index << " " << _info->name);
	list->push_back(info);
}

// to not update all the time ...
static std::vector<audio::orchestra::DeviceInfo> pulseAudioListOfDevice;
static audio::Time pulseAudioListOfDeviceTime;

std::vector<audio::orchestra::DeviceInfo> audio::orchestra::api::pulse::getDeviceList() {
	audio::Duration delta = audio::Time::now() - pulseAudioListOfDeviceTime;
	if (delta < audio::Duration(30,0)) {
		return pulseAudioListOfDevice;
	}
	// Define our pulse audio loop and connection variables
	pa_mainloop* pulseAudioMainLoop;
	pa_mainloop_api* pulseAudioMainLoopAPI;
	pa_operation* pulseAudioOperation;
	pa_context* pulseAudioContex;
	pa_context_flags_t pulseAudioFlags = PA_CONTEXT_NOAUTOSPAWN;
	std::vector<audio::orchestra::DeviceInfo>& out = pulseAudioListOfDevice;
	out.clear();
	// We'll need these state variables to keep track of our requests
	int state = 0;
	int pulseAudioReady = 0;
	// Create a mainloop API and connection to the default server
	pulseAudioMainLoop = pa_mainloop_new();
	pulseAudioMainLoopAPI = pa_mainloop_get_api(pulseAudioMainLoop);
	pulseAudioContex = pa_context_new(pulseAudioMainLoopAPI, "orchestraPulseCount");
	// If there's an error, the callback will set pulseAudioReady
	pa_context_set_state_callback(pulseAudioContex, callbackStateMachine, &pulseAudioReady);
	// This function connects to the pulse server
	pa_context_connect(pulseAudioContex, NULL, pulseAudioFlags, NULL);
	bool playLoop = true;
	while (playLoop == true) {
		// We can't do anything until PA is ready, so just iterate the mainloop
		// and continue
		if (pulseAudioReady == 0) {
			pa_mainloop_iterate(pulseAudioMainLoop, 1, nullptr);
			continue;
		}
		// We couldn't get a connection to the server, so exit out
		if (pulseAudioReady == 2) {
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
				ATA_DEBUG("Request sink list");
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
					ATA_DEBUG("Request sources list");
					pulseAudioOperation = pa_context_get_source_info_list(pulseAudioContex,
					                                                      callbackGetSourceList,
					                                                      &out);
					state++;
				}
				break;
			case 2:
				if (pa_operation_get_state(pulseAudioOperation) == PA_OPERATION_DONE) {
					ATA_DEBUG("All is done");
					// Now we're done, clean up and disconnect and return
					pa_operation_unref(pulseAudioOperation);
					pa_context_disconnect(pulseAudioContex);
					pa_context_unref(pulseAudioContex);
					pa_mainloop_free(pulseAudioMainLoop);
					playLoop = false;
					break;
				}
			break;
			default:
				// We should never see this state
				ATA_ERROR("Error in getting the devices list ...");
				return out;
		}
		// Iterate the main loop ..
		if (playLoop == true) {
			pa_mainloop_iterate(pulseAudioMainLoop, 1, nullptr);
		}
	}
	// TODO: need to do it better ...
	// set default device:
	int32_t idInput = -1;
	int32_t idOutput = -1;
	for (int32_t iii=0; iii<out.size(); ++iii) {
		if (out[iii].input == true) {
			if (idInput != -1) {
				continue;
			}
			if (etk::end_with(out[iii].name, ".monitor", false) == false) {
				idInput = iii;
				out[iii].isDefault = true;
			}
		} else {
			if (idOutput != -1) {
				continue;
			}
			if (etk::end_with(out[iii].name, ".monitor", false) == false) {
				idOutput = iii;
				out[iii].isDefault = true;
			}
		}
	}
	return out;
}

#endif
