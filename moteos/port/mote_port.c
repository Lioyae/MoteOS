#include "mote.h"

#ifdef MOTE_PORT_HOST

void mote_idle(void)
{
}

#else

void SysTick_Handler(void)
{
    mote_tick();
}

void mote_idle(void)
{
    __WFI();
}

#endif
