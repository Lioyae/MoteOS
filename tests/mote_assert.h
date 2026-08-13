/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTE_TEST_ASSERT_H
#define MOTE_TEST_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

/* CI 断言构建专用：mote_config.h 里 MOTE_ASSERT 默认是 ((void)0)，
 * 断言路径（配置边界校验、队列溢出防护）永远不会被编译。
 * 本头文件通过 -include 强制在全部编译单元最前面定义 MOTE_ASSERT，
 * 使断言路径被真实编译并参与运行：测试全程不应触发任何断言，
 * 一旦触发即打印位置并终止（CTest 报红）。仅宿主机测试构建使用。 */

#ifndef MOTE_ASSERT
#define MOTE_ASSERT(x)                                                         \
    do {                                                                       \
        if (!(x)) {                                                            \
            printf("  MOTE_ASSERT failed %s:%d: %s\n", __FILE__, __LINE__,     \
                   #x);                                                        \
            fflush(stdout);                                                    \
            abort();                                                           \
        }                                                                      \
    } while (0)
#endif

#endif
