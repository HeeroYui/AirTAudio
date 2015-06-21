/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 */

#include <jni.h>
#include <pthread.h>
#include <mutex>
#include <audio/orchestra/debug.h>
#include <audio/orchestra/api/AndroidNativeInterface.h>
/* include auto generated file */
#include <org_musicdsp_orchestra_Constants.h>
#include <jvm-basics/jvm-basics.h>
#include <etk/memory.h>
#include <etk/tool.h>

class AndroidOrchestraContext {
	public:
		AndroidOrchestraContext(JNIEnv* _env,
		                        jclass _classBase,
		                        jobject _objCallback) {
			ATA_ERROR("kjlkjlk");
		}
};

static std::shared_ptr<AndroidOrchestraContext> s_localContext;
static int32_t s_nbContextRequested(0);

int32_t ttttttt() {
	return etk::tool::irand(0,54456);
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

