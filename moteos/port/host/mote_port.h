#ifndef MOTE_PORT_H
#define MOTE_PORT_H

/* 主机（PC 单元测试）移植：临界区为空操作 */
#define MOTE_PORT_HOST

#define MOTE_ENTER_CRITICAL() ((void)0)
#define MOTE_EXIT_CRITICAL()  ((void)0)

#endif
