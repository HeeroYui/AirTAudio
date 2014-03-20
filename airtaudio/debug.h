/**
 * @author Edouard DUPIN
 * 
 * @copyright 2011, Edouard DUPIN, all right reserved
 * 
 * @license BSD 3 clauses (see license file)
 */

#ifndef __EAUDIOFX_DEBUG_H__
#define __EAUDIOFX_DEBUG_H__

#include <etk/types.h>
#include <etk/debugGeneric.h>

extern const char * airtaudioLibName;

#define ATA_CRITICAL(data)    ETK_CRITICAL(airtaudioLibName, data)
#define ATA_WARNING(data)     ETK_WARNING(airtaudioLibName, data)
#define ATA_ERROR(data)       ETK_ERROR(airtaudioLibName, data)
#define ATA_INFO(data)        ETK_INFO(airtaudioLibName, data)
#define ATA_DEBUG(data)       ETK_DEBUG(airtaudioLibName, data)
#define ATA_VERBOSE(data)     ETK_VERBOSE(airtaudioLibName, data)
#define ATA_ASSERT(cond,data) ETK_ASSERT(airtaudioLibName, cond, data)
#define ATA_CHECK_INOUT(cond) ETK_CHECK_INOUT(airtaudioLibName, cond)
#define ATA_TODO(cond)        ETK_TODO(airtaudioLibName, cond)

#endif

