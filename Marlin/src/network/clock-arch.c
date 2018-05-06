#include "clock-arch.h"
#include "LPC17xx.h"

static clock_time_t Ticks;

void clock_tick(void)
{
  ++Ticks;
}

/* returned The current clock time, measured in system ticks */
clock_time_t clock_time(void)
{
  return(Ticks);
}
