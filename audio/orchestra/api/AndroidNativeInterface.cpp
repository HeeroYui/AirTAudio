/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 */

#include <jni.h>
#include <pthread.h>
#include <mutex>
#include <audio/orchestra/debug.h>
#include <audio/orchestra/error.h>
#include <audio/orchestra/api/AndroidNativeInterface.h>
/* include auto generated file */
#include <org_musicdsp_orchestra_Constants.h>
#include <jvm-basics/jvm-basics.h>
#include <etk/memory.h>
#include <ejson/ejson.h>

class AndroidOrchestraContext {
	public:
		// get a resources from the java environement : 
		JNIEnv* m_JavaVirtualMachinePointer; //!< the JVM
		jclass m_javaClassOrchestra; //!< main activity class (android ...)
		jclass m_javaClassOrchestraCallback;
		jobject m_javaObjectOrchestraCallback;
		jmethodID m_javaMethodOrchestraActivityAudioGetDeviceCount;
		jmethodID m_javaMethodOrchestraActivityAudioGetDeviceProperty;
		jmethodID m_javaMethodOrchestraActivityAudioOpenDevice;
		jmethodID m_javaMethodOrchestraActivityAudioCloseDevice;
		jmethodID m_javaMethodOrchestraActivityAudioStart;
		jmethodID m_javaMethodOrchestraActivityAudioStop;
		jclass m_javaDefaultClassString; //!< default string class
	private:
		bool safeInitMethodID(jmethodID& _mid, jclass& _cls, const char* _name, const char* _sign) {
			_mid = m_JavaVirtualMachinePointer->GetMethodID(_cls, _name, _sign);
			if(_mid == nullptr) {
				ATA_ERROR("C->java : Can't find the method " << _name);
				/* remove access on the virtual machine : */
				m_JavaVirtualMachinePointer = nullptr;
				return false;
			}
			return true;
		}
		bool java_attach_current_thread(int *_rstatus) {
			ATA_DEBUG("C->java : call java");
			if (jvm_basics::getJavaVM() == nullptr) {
				ATA_ERROR("C->java : JVM not initialised");
				m_JavaVirtualMachinePointer = nullptr;
				return false;
			}
			*_rstatus = jvm_basics::getJavaVM()->GetEnv((void **) &m_JavaVirtualMachinePointer, JNI_VERSION_1_6);
			if (*_rstatus == JNI_EDETACHED) {
				JavaVMAttachArgs lJavaVMAttachArgs;
				lJavaVMAttachArgs.version = JNI_VERSION_1_6;
				lJavaVMAttachArgs.name = "EwolNativeThread";
				lJavaVMAttachArgs.group = nullptr; 
				int status = jvm_basics::getJavaVM()->AttachCurrentThread(&m_JavaVirtualMachinePointer, &lJavaVMAttachArgs);
				jvm_basics::checkExceptionJavaVM(m_JavaVirtualMachinePointer);
				if (status != JNI_OK) {
					ATA_ERROR("C->java : AttachCurrentThread failed : " << status);
					m_JavaVirtualMachinePointer = nullptr;
					return false;
				}
			}
			return true;
		}
		void java_detach_current_thread(int _status) {
			if(_status == JNI_EDETACHED) {
				jvm_basics::getJavaVM()->DetachCurrentThread();
				m_JavaVirtualMachinePointer = nullptr;
			}
		}
		
	public:
		AndroidOrchestraContext(JNIEnv* _env, jclass _classBase, jobject _objCallback) :
		  m_JavaVirtualMachinePointer(nullptr),
		  m_javaClassOrchestra(0),
		  m_javaClassOrchestraCallback(0),
		  m_javaObjectOrchestraCallback(0),
		  
		  m_javaMethodOrchestraActivityAudioGetDeviceCount(0),
		  m_javaMethodOrchestraActivityAudioGetDeviceProperty(0),
		  m_javaMethodOrchestraActivityAudioOpenDevice(0),
		  m_javaMethodOrchestraActivityAudioCloseDevice(0),
		  m_javaMethodOrchestraActivityAudioStart(0),
		  m_javaMethodOrchestraActivityAudioStop(0),
		  m_javaDefaultClassString(0) {
			ATA_DEBUG("*******************************************");
			ATA_DEBUG("** set JVM Pointer (orchestra)           **");
			ATA_DEBUG("*******************************************");
			m_JavaVirtualMachinePointer = _env;
			// get default needed all time elements : 
			if (m_JavaVirtualMachinePointer == nullptr) {
				ATA_ERROR("C->java: NULLPTR jvm interface");
				return;
			}
			ATA_DEBUG("C->java: try load org/musicdsp/orchestra/Orchestra class");
			m_javaClassOrchestra = m_JavaVirtualMachinePointer->FindClass("org/musicdsp/orchestra/Orchestra" );
			if (m_javaClassOrchestra == 0) {
				ATA_ERROR("C->java : Can't find org/musicdsp/orchestra/Orchestra class");
				// remove access on the virtual machine : 
				m_JavaVirtualMachinePointer = nullptr;
				return;
			}
			/* The object field extends Activity and implement OrchestraCallback */
			m_javaClassOrchestraCallback = m_JavaVirtualMachinePointer->GetObjectClass(_objCallback);
			if(m_javaClassOrchestraCallback == nullptr) {
				ATA_ERROR("C->java : Can't find org/musicdsp/orchestra/ManagerCallback class");
				// remove access on the virtual machine : 
				m_JavaVirtualMachinePointer = nullptr;
				return;
			}
			bool functionCallbackIsMissing = false;
			bool ret= false;
			
			ret = safeInitMethodID(m_javaMethodOrchestraActivityAudioGetDeviceCount,
			                       m_javaClassOrchestraCallback,
			                       "getDeviceCount",
			                       "()I");
			if (ret == false) {
				jvm_basics::checkExceptionJavaVM(_env);
				ATA_ERROR("system can not start without function : getDeviceCount");
				functionCallbackIsMissing = true;
			}
			
			ret = safeInitMethodID(m_javaMethodOrchestraActivityAudioGetDeviceProperty,
			                       m_javaClassOrchestraCallback,
			                       "getDeviceProperty",
			                       "(I)Ljava/lang/String;");
			if (ret == false) {
				jvm_basics::checkExceptionJavaVM(_env);
				ATA_ERROR("system can not start without function : getDeviceProperty");
				functionCallbackIsMissing = true;
			}
			ret = safeInitMethodID(m_javaMethodOrchestraActivityAudioOpenDevice,
			                       m_javaClassOrchestraCallback,
			                       "openDevice",
			                       "(IIII)Z");
			if (ret == false) {
				jvm_basics::checkExceptionJavaVM(_env);
				ATA_ERROR("system can not start without function : openDevice");
				functionCallbackIsMissing = true;
			}
			ret = safeInitMethodID(m_javaMethodOrchestraActivityAudioCloseDevice,
			                       m_javaClassOrchestraCallback,
			                       "closeDevice",
			                       "(I)Z");
			if (ret == false) {
				jvm_basics::checkExceptionJavaVM(_env);
				ATA_ERROR("system can not start without function : closeDevice");
				functionCallbackIsMissing = true;
			}
			ret = safeInitMethodID(m_javaMethodOrchestraActivityAudioStart,
			                       m_javaClassOrchestraCallback,
			                       "start",
			                       "(I)Z");
			if (ret == false) {
				jvm_basics::checkExceptionJavaVM(_env);
				ATA_ERROR("system can not start without function : start");
				functionCallbackIsMissing = true;
			}
			ret = safeInitMethodID(m_javaMethodOrchestraActivityAudioStop,
			                       m_javaClassOrchestraCallback,
			                       "stop",
			                       "(I)Z");
			if (ret == false) {
				jvm_basics::checkExceptionJavaVM(_env);
				ATA_ERROR("system can not start without function : stop");
				functionCallbackIsMissing = true;
			}
			
			
			m_javaObjectOrchestraCallback = _env->NewGlobalRef(_objCallback);
			if (m_javaObjectOrchestraCallback == nullptr) {
				functionCallbackIsMissing = true;
			}
			
			m_javaDefaultClassString = m_JavaVirtualMachinePointer->FindClass("java/lang/String" );
			if (m_javaDefaultClassString == 0) {
				ATA_ERROR("C->java : Can't find java/lang/String" );
				// remove access on the virtual machine : 
				m_JavaVirtualMachinePointer = nullptr;
				functionCallbackIsMissing = true;
			}
			if (functionCallbackIsMissing == true) {
				ATA_CRITICAL(" mission one function ==> system can not work withut it...");
			}
		}
		
		~AndroidOrchestraContext() {
			// TODO ...
		}
		
		void unInit(JNIEnv* _env) {
			_env->DeleteGlobalRef(m_javaObjectOrchestraCallback);
			m_javaObjectOrchestraCallback = nullptr;
		}
		
		uint32_t getDeviceCount() {
			// Request the clipBoard :
			ATA_DEBUG("C->java : audio get device count");
			int status;
			if(!java_attach_current_thread(&status)) {
				return 0;
			}
			ATA_DEBUG("Call CallIntMethod ...");
			//Call java ...
			jint ret = m_JavaVirtualMachinePointer->CallIntMethod(m_javaObjectOrchestraCallback, m_javaMethodOrchestraActivityAudioGetDeviceCount);
			// manage execption : 
			jvm_basics::checkExceptionJavaVM(m_JavaVirtualMachinePointer);
			java_detach_current_thread(status);
			return (uint32_t)ret;
			return 0;
		}
		audio::orchestra::DeviceInfo getDeviceInfo(uint32_t _idDevice) {
			audio::orchestra::DeviceInfo info;
			// Request the clipBoard :
			ATA_DEBUG("C->java : audio get device count");
			int status;
			if(!java_attach_current_thread(&status)) {
				return info;
			}
			//Call java ...
			jstring returnString = (jstring) m_JavaVirtualMachinePointer->CallObjectMethod(m_javaObjectOrchestraCallback, m_javaMethodOrchestraActivityAudioGetDeviceProperty, _idDevice);
			const char *js = m_JavaVirtualMachinePointer->GetStringUTFChars(returnString, nullptr);
			std::string retString(js);
			m_JavaVirtualMachinePointer->ReleaseStringUTFChars(returnString, js);
			//m_JavaVirtualMachinePointer->DeleteLocalRef(returnString);
			// manage execption : 
			jvm_basics::checkExceptionJavaVM(m_JavaVirtualMachinePointer);
			java_detach_current_thread(status);
			ATA_WARNING("get device information : " << retString);
			ejson::Document doc;
			if (doc.parse(retString) == false) {
				return info;
			}
			info.name = doc.getStringValue("name", "no-name");
			if (doc.getStringValue("type", "output") == "output") {
				info.input = false;
			} else {
				info.input = true;
			}
			std11::shared_ptr<const ejson::Array> list = doc.getArray("sample-rate");
			if (list != nullptr) {
				for (size_t iii=0; iii<list->size(); ++iii) {
					info.sampleRates.push_back(int32_t(list->getNumberValue(iii, 48000)));
				}
			}
			list = doc.getArray("channels");
			if (list != nullptr) {
				for (size_t iii=0; iii<list->size(); ++iii) {
					info.channels.push_back(audio::getChannelFromString(list->getStringValue(iii, "???")));
				}
			}
			list = doc.getArray("format");
			if (list != nullptr) {
				for (size_t iii=0; iii<list->size(); ++iii) {
					info.nativeFormats.push_back(audio::getFormatFromString(list->getStringValue(iii, "???")));
				}
			}
			info.isDefault = doc.getBooleanValue("default", false);
			/*
			       + "	sample-rate:[8000,16000,24000,32000,48000,96000],\n"
			       + "	channels=[front-left,front-right],\n"
			       + "	format:[int16]\n"
			*/
			//return retString;
			return info;
		}
	private:
		//AndroidAudioCallback m_audioCallBack;
		//void* m_audioCallBackUserData;
	public:
		int32_t open(uint32_t _idDevice,
		             audio::orchestra::mode _mode,
		             uint32_t _channels,
		             uint32_t _firstChannel,
		             uint32_t _sampleRate,
		             audio::format _format,
		             uint32_t *_bufferSize,
		             const audio::orchestra::StreamOptions& _options) {
			ATA_DEBUG("C->java : audio open device");
			int status;
			if(!java_attach_current_thread(&status)) {
				return -1;
			}
			//Call java ...
			jboolean ret = m_JavaVirtualMachinePointer->CallBooleanMethod(m_javaObjectOrchestraCallback, m_javaMethodOrchestraActivityAudioOpenDevice, _idDevice, _sampleRate, _channels, /*_format*/ 1);
			// manage execption : 
			jvm_basics::checkExceptionJavaVM(m_JavaVirtualMachinePointer);
			java_detach_current_thread(status);
			if (bool(ret) == true) {
				return 0;
			}
			return -1;
		}
	public:
		enum audio::orchestra::error closeStream(int32_t _id) {
			ATA_DEBUG("C->java : audio close device");
			int status;
			if(!java_attach_current_thread(&status)) {
				return audio::orchestra::error_fail;
			}
			//Call java ...
			jboolean ret = m_JavaVirtualMachinePointer->CallBooleanMethod(m_javaObjectOrchestraCallback, m_javaMethodOrchestraActivityAudioCloseDevice, _id);
			// manage execption : 
			jvm_basics::checkExceptionJavaVM(m_JavaVirtualMachinePointer);
			java_detach_current_thread(status);
			if (bool(ret) == false) {
				return audio::orchestra::error_fail;
			}
			return audio::orchestra::error_none;
		}
		
		enum audio::orchestra::error startStream(int32_t _id) {
			ATA_DEBUG("C->java : audio start device");
			int status;
			if(!java_attach_current_thread(&status)) {
				return audio::orchestra::error_fail;
			}
			//Call java ...
			jboolean ret = m_JavaVirtualMachinePointer->CallBooleanMethod(m_javaObjectOrchestraCallback, m_javaMethodOrchestraActivityAudioStart, _id);
			// manage execption : 
			jvm_basics::checkExceptionJavaVM(m_JavaVirtualMachinePointer);
			java_detach_current_thread(status);
			if (bool(ret) == false) {
				return audio::orchestra::error_fail;
			}
			return audio::orchestra::error_none;
		}
		enum audio::orchestra::error stopStream(int32_t _id) {
			
			ATA_DEBUG("C->java : audio close device");
			int status;
			if(!java_attach_current_thread(&status)) {
				return audio::orchestra::error_fail;
			}
			//Call java ...
			jboolean ret = m_JavaVirtualMachinePointer->CallBooleanMethod(m_javaObjectOrchestraCallback, m_javaMethodOrchestraActivityAudioStop, _id);
			// manage execption : 
			jvm_basics::checkExceptionJavaVM(m_JavaVirtualMachinePointer);
			java_detach_current_thread(status);
			if (bool(ret) == false) {
				return audio::orchestra::error_fail;
			}
			return audio::orchestra::error_none;
		}
		enum audio::orchestra::error abortStream(int32_t _id) {
			return audio::orchestra::error_fail;
		}
};

static std::shared_ptr<AndroidOrchestraContext> s_localContext;
static int32_t s_nbContextRequested(0);


uint32_t audio::orchestra::api::android::getDeviceCount() {
	if (s_localContext == nullptr) {
		return 0;
	}
	return s_localContext->getDeviceCount();
}

audio::orchestra::DeviceInfo audio::orchestra::api::android::getDeviceInfo(uint32_t _device) {
	if (s_localContext == nullptr) {
		return audio::orchestra::DeviceInfo();
	}
	return s_localContext->getDeviceInfo(_device);
}

int32_t audio::orchestra::api::android::open(uint32_t _device,
                                             audio::orchestra::mode _mode,
                                             uint32_t _channels,
                                             uint32_t _firstChannel,
                                             uint32_t _sampleRate,
                                             audio::format _format,
                                             uint32_t *_bufferSize,
                                             const audio::orchestra::StreamOptions& _options) {
	if (s_localContext == nullptr) {
		return -1;
	}
	return s_localContext->open(_device, _mode, _channels, _firstChannel, _sampleRate, _format, _bufferSize, _options);
}

enum audio::orchestra::error audio::orchestra::api::android::closeStream(int32_t _id) {
	if (s_localContext == nullptr) {
		return audio::orchestra::error_fail;
	}
	return s_localContext->closeStream(_id);
}

enum audio::orchestra::error audio::orchestra::api::android::startStream(int32_t _id) {
	if (s_localContext == nullptr) {
		return audio::orchestra::error_fail;
	}
	return s_localContext->startStream(_id);
}

enum audio::orchestra::error audio::orchestra::api::android::stopStream(int32_t _id) {
	if (s_localContext == nullptr) {
		return audio::orchestra::error_fail;
	}
	return s_localContext->stopStream(_id);
}

enum audio::orchestra::error audio::orchestra::api::android::abortStream(int32_t _id) {
	if (s_localContext == nullptr) {
		return audio::orchestra::error_fail;
	}
	return s_localContext->abortStream(_id);
}

extern "C" {
	void Java_org_musicdsp_orchestra_Orchestra_NNsetJavaManager(JNIEnv* _env,
	                                                            jclass _classBase,
	                                                            jobject _objCallback) {
		std::unique_lock<std::mutex> lock(jvm_basics::getMutexJavaVM());
		ATA_INFO("*******************************************");
		ATA_INFO("** Creating Orchestra context            **");
		ATA_INFO("*******************************************");
		if (s_localContext != nullptr) {
			s_nbContextRequested++;
		}
		s_localContext = std::make_shared<AndroidOrchestraContext>(_env, _classBase, _objCallback);
		if (s_localContext == nullptr) {
			ATA_ERROR("Can not allocate the orchestra main context instance");
			return;
		}
		s_nbContextRequested++;
	}
	
	void Java_org_musicdsp_orchestra_Orchestra_NNsetJavaManagerRemove(JNIEnv* _env, jclass _cls) {
		std::unique_lock<std::mutex> lock(jvm_basics::getMutexJavaVM());
		ATA_INFO("*******************************************");
		ATA_INFO("** remove Orchestra Pointer              **");
		ATA_INFO("*******************************************");
		if (s_nbContextRequested == 0) {
			ATA_ERROR("Request remove orchestra interface from Android, but no more interface availlable");
			return;
		}
		s_nbContextRequested--;
		if (s_nbContextRequested == 0) {
			s_localContext.reset();
		}
	}
}

