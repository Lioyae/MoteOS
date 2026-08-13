# MoteOS - event-driven cooperative kernel for small MCUs
# Copyright (c) 2026 Lioyae
# https://github.com/Lioyae/MoteOS
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(moteos C)

# 宿主机测试依赖 GCC 兼容编译器：C99 指定初始化器、-Wall/-Wextra/-Werror、
# 断言构建的 -include 强制头均非 MSVC 语义（MSVC 下会报 D8021 等晦涩错误）。
# Windows 请使用 MinGW 生成器：
#   cmake -S . -B build -G "MinGW Makefiles"
if(MSVC)
    message(FATAL_ERROR
        "MoteOS host tests require a GCC-compatible compiler. "
        "On Windows, configure with: cmake -S . -B build -G \"MinGW Makefiles\"")
endif()

enable_testing()

set(MOTEOS_TEST_SOURCES
    moteos/mote.c
    moteos/mote_task.c
    moteos/mote_mail.c
    moteos/port/mote_port.c
    tests/main.c
    tests/test_queue.c
    tests/test_timer.c
    tests/test_task.c
    tests/test_mail.c
    tests/test_interleave.c
)

add_executable(test_moteos ${MOTEOS_TEST_SOURCES})

target_include_directories(test_moteos PRIVATE
    moteos
    moteos/port/host
    tests
)

target_compile_definitions(test_moteos PRIVATE MOTE_TEST_INJECT_ENABLE=1)

target_compile_options(test_moteos PRIVATE -Wall -Wextra -Werror)

add_test(NAME moteos_unit COMMAND test_moteos)

# 断言开启构建：默认 MOTE_ASSERT 为 ((void)0)，断言路径从不参与编译。
# 强制 -include tests/mote_assert.h（在 mote_config.h 之前生效），
# 使内核断言路径被真实编译并运行；测试全程不应触发任何断言。
add_executable(test_moteos_assert ${MOTEOS_TEST_SOURCES})

target_include_directories(test_moteos_assert PRIVATE
    moteos
    moteos/port/host
    tests
)

target_compile_definitions(test_moteos_assert PRIVATE MOTE_TEST_INJECT_ENABLE=1)

target_compile_options(test_moteos_assert PRIVATE -Wall -Wextra -Werror
    -include ${CMAKE_CURRENT_SOURCE_DIR}/tests/mote_assert.h)

add_test(NAME moteos_unit_assert COMMAND test_moteos_assert)

# 最坏配置构建：队列 255 / 延时槽 16 / 任务槽 16。
# 验证极端配置下（临界区最长路径：replace 全扫描 255、大队列回绕）
# 编译与并发语义正确性；与 CI 交叉编译的 -DMOTE_EVT_QUEUE_SIZE=255 对应。
add_executable(test_moteos_max ${MOTEOS_TEST_SOURCES})

target_include_directories(test_moteos_max PRIVATE
    moteos
    moteos/port/host
    tests
)

target_compile_definitions(test_moteos_max PRIVATE
    MOTE_TEST_INJECT_ENABLE=1
    MOTE_EVT_QUEUE_SIZE=255
    MOTE_DELAYED_MAX=16
    MOTE_TASK_SLOT_MAX=16
)

target_compile_options(test_moteos_max PRIVATE -Wall -Wextra -Werror)

add_test(NAME moteos_unit_max COMMAND test_moteos_max)
