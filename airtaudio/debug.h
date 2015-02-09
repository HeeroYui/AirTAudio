/** @file
 * @author Edouard DUPIN 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * @license APACHE v2.0 (see license file)
 * @fork from RTAudio
 */

#ifndef __AIRTAUDIO_DEBUG_H__
#define __AIRTAUDIO_DEBUG_H__

#include <etk/log.h>

namespace airtaudio {
	int32_t getLogId();
};
// TODO : Review this problem of multiple intanciation of "std::stringbuf sb"
#define ATA_BASE(info,data) \
	do { \
		if (info <= etk::log::getLevel(airtaudio::getLogId())) { \
			std::stringbuf sb; \
			std::ostream tmpStream(&sb); \
			tmpStream << data; \
			etk::log::logStream(airtaudio::getLogId(), info, __LINE__, __class__, __func__, tmpStream); \
		} \
	} while(0)

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

