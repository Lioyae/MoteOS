/*
 * MoteOS - event-driven cooperative kernel for small MCUs
 * Copyright (c) 2026 Lioyae
 * https://github.com/Lioyae/MoteOS
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MOTE_TEST_H
#define MOTE_TEST_H

#include <stdio.h>

extern int g_asserts;
extern int g_fails;

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        g_asserts++;                                                           \
        if (!(cond)) {                                                         \
            g_fails++;                                                         \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                      \
    } while (0)

#endif
