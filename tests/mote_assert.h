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

/* CI 断言构建专用：mote_config.h 里 MOTE_ASSERT 默认已开启（回调
 * mote_assert_fail，默认实现停机/abort）。本头文件通过 -include 强制在
 * 全部编译单元最前面定义 MOTE_ASSERT 为 abort 版本，保证断言路径被真实
 * 编译并参与运行，且触发即打印位置并终止（CTest 报红）。仅宿主机测试
 * 构建使用。 */

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
