/**
 * @author Gary P. SCAVONE
 * 
 * @copyright 2001-2013 Gary P. Scavone, all right reserved
 * 
 * @license like MIT (see license file)
 */

#ifndef __AIRTAUDIO_INT24_T_H__
#define __AIRTAUDIO_INT24_T_H__

#pragma pack(push, 1)
class int24_t {
	protected:
		uint8_t c3[3];
	public:
		int24_t(void) {}
		int24_t& operator = (const int32_t& i) {
			c3[0] = (i & 0x000000ff);
			c3[1] = (i & 0x0000ff00) >> 8;
			c3[2] = (i & 0x00ff0000) >> 16;
			return *this;
		}
		
		int24_t(const int24_t& v) {
			*this = v;
		}
		int24_t(const double& d) {
			*this = (int32_t)d;
		}
		int24_t(const float& f) {
			*this = (int32_t)f;
		}
		int24_t(const int16_t& s) {
			*this = (int32_t)s;
		}
		int24_t(const int8_t& c) {
			*this = (int32_t)c;
		}
		
		int32_t asInt(void) {
			int32_t i = c3[0] | (c3[1] << 8) | (c3[2] << 16);
			if (i & 0x800000) {
				i |= ~0xffffff;
			}
			return i;
		}
};
#pragma pack(pop)


#endif
