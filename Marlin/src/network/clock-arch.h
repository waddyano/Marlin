#ifndef __CLOCK_ARCH_H__
#define __CLOCK_ARCH_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int clock_time_t;

#define CLOCK_CONF_SECOND 100   // tick number every second

extern void clock_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* __CLOCK_ARCH_H__ */
