/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AIRTAUDIO_API_PULSE_H__) && defined(__LINUX_PULSE__)
#define __AIRTAUDIO_API_PULSE_H__

namespace airtaudio {
	namespace api {
		class PulsePrivate;
		class Pulse: public airtaudio::Api {
			public:
				static airtaudio::Api* Create();
			public:
				Pulse();
				virtual ~Pulse();
				enum airtaudio::type getCurrentApi() {
					return airtaudio::type_pulse;
				}
				uint32_t getDeviceCount();
				airtaudio::DeviceInfo getDeviceInfo(uint32_t _device);
				enum airtaudio::error closeStream();
				enum airtaudio::error startStream();
				enum airtaudio::error stopStream();
				enum airtaudio::error abortStream();
				// This function is intended for internal use only.	It must be
				// public because it is called by the internal callback handler,
				// which is not a member of RtAudio.	External use of this function
				// will most likely produce highly undesireable results!
				void callbackEventOneCycle();
				void callbackEvent();
			private:
				std11::shared_ptr<PulsePrivate> m_private;
				std::vector<airtaudio::DeviceInfo> m_devices;
				void saveDeviceInfo();
				bool probeDeviceOpen(uint32_t _device,
				                     airtaudio::mode _mode,
				                     uint32_t _channels,
				                     uint32_t _firstChannel,
				                     uint32_t _sampleRate,
				                     audio::format _format,
				                     uint32_t *_bufferSize,
				                     const airtaudio.::StreamOptions& _options);
		};
	};
};

#endif
