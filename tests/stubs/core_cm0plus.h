/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */

/* CI 交叉编译专用桩头文件：仅为语法/体积检查提供最小声明，
 * 不参与任何真实编译，不能用于芯片工程 */

#ifndef CORE_CM0PLUS_STUB_H
#define CORE_CM0PLUS_STUB_H

#include <stdint.h>

static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline void __set_PRIMASK(uint32_t p) { (void)p; }
static inline void __disable_irq(void) { }
static inline void __enable_irq(void) { }
static inline void __WFI(void) { }

#endif
