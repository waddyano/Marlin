#ifndef __CLOCK_ARCH_H__
#define __CLOCK_ARCH_H__

#include "lpc_types.h"

typedef unsigned int clock_time_t;

extern void clock_tick();

#define CLOCK_CONF_SECOND 100   // tick number every second

#endif /* __CLOCK_ARCH_H__ */
