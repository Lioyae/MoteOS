# MoteOS - event-driven cooperative kernel for small MCUs
# Copyright (c) 2026 Lioyae
# https://github.com/Lioyae/MoteOS
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(moteos C)

enable_testing()

add_executable(test_moteos
    moteos/mote.c
    moteos/mote_task.c
    moteos/mote_mail.c
    moteos/port/mote_port.c
    tests/main.c
    tests/test_queue.c
    tests/test_timer.c
    tests/test_task.c
    tests/test_mail.c
)

target_include_directories(test_moteos PRIVATE
    moteos
    moteos/port/host
    tests
)

target_compile_options(test_moteos PRIVATE -Wall -Wextra)

add_test(NAME moteos_unit COMMAND test_moteos)


