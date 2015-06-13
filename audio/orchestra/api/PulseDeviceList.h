/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#if !defined(__AUDIO_ORCHESTRA_API_PULSE_DEVICE_H__) && defined(ORCHESTRA_BUILD_PULSE)
#define __AUDIO_ORCHESTRA_API_PULSE_DEVICE_H__

#include <etk/types.h>

namespace audio {
	namespace orchestra {
		namespace api {
			namespace pulse {
				class Element {
					private:
						size_t m_index;
						bool m_input;
						std::string m_name;
						std::string m_description;
					public:
						Element(size_t _index, bool _input, const std::string& _name, const std::string& _desc) :
						  m_index(_index),
						  m_input(_input),
						  m_name(_name),
						  m_description(_desc) {
							// nothing to do...
						}
						size_t getIndex() const {
							return m_index;
						}
						bool isInput() const {
							return m_input;
						}
						const std::string& getName() const {
							return m_name;
						}
						const std::string& getDescription() const {
							return m_description;
						}
				};
				std::vector<audio::orchestra::api::pulse::Element> getDeviceList();
			}
		}
	}
}

#endif