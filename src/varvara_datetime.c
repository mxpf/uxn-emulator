#include "varvara_internal.h"

#include <time.h>

uint8_t
varvara_datetime_read(Varvara *varvara, uint8_t port)
{
	time_t now = time(NULL);
	struct tm broken_down;
	struct tm *value;
	uint16_t number;

#if defined(_WIN32)
	if(localtime_s(&broken_down, &now) != 0)
		return 0;
	value = &broken_down;
#else
	value = localtime(&now);
	if(!value)
		return 0;
	broken_down = *value;
	value = &broken_down;
#endif
	switch(port) {
	case 0xc0:
		number = (uint16_t)(value->tm_year + 1900);
		varvara_poke_short(&varvara->uxn.devices[0xc0], number);
		break;
	case 0xc2: varvara->uxn.devices[port] = (uint8_t)value->tm_mon; break;
	case 0xc3: varvara->uxn.devices[port] = (uint8_t)value->tm_mday; break;
	case 0xc4: varvara->uxn.devices[port] = (uint8_t)value->tm_hour; break;
	case 0xc5: varvara->uxn.devices[port] = (uint8_t)value->tm_min; break;
	case 0xc6: varvara->uxn.devices[port] = (uint8_t)value->tm_sec; break;
	case 0xc7: varvara->uxn.devices[port] = (uint8_t)value->tm_wday; break;
	case 0xc8:
		number = (uint16_t)value->tm_yday;
		varvara_poke_short(&varvara->uxn.devices[0xc8], number);
		break;
	case 0xca: varvara->uxn.devices[port] = (uint8_t)value->tm_isdst; break;
	default: break;
	}
	return varvara->uxn.devices[port];
}
