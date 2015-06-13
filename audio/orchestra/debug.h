/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AUDIO_ORCHESTRA_DEBUG_H__
#define __AUDIO_ORCHESTRA_DEBUG_H__

#include <etk/log.h>

namespace audio {
	namespace orchestra {
		int32_t getLogId();
	}
}
#define ATA_BASE(info,data) TK_LOG_BASE(audio::orchestra::getLogId(),info,data)

#define ATA_PRINT(data)      ATA_BASE(-1, data)
#define ATA_CRITICAL(data)      ATA_BASE(1, data)
#define ATA_ERROR(data)         ATA_BASE(2, data)
#define ATA_WARNING(data)       ATA_BASE(3, data)
#ifdef DEBUG
	#define ATA_INFO(data)          ATA_BASE(4, data)
	#define ATA_DEBUG(data)         ATA_BASE(5, data)
	#define ATA_VERBOSE(data)       ATA_BASE(6, data)
	#define ATA_TODO(data)          ATA_BASE(4, "TODO : " << data)
#else
	#define ATA_INFO(data)          do { } while(false)
	#define ATA_DEBUG(data)         do { } while(false)
	#define ATA_VERBOSE(data)       do { } while(false)
	#define ATA_TODO(data)          do { } while(false)
#endif

#define ATA_ASSERT(cond,data) \
	do { \
		if (!(cond)) { \
			ATA_CRITICAL(data); \
			assert(!#cond); \
		} \
	} while (0)

#endif

