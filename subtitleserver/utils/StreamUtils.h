/*
 * Copyright (C) 2014-2025 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <time.h>

#define RETURN_CASE_STR(x)                                              \
  case x:                                                               \
    return #x

static inline uint64_t getCurrentTimeMs()
{
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / (1000 * 1000);
}

static inline int64_t pts2Ms(int64_t pts) {
    return pts/90;
}

static inline int64_t ms2Pts(int64_t ms) {
    return ms*90;
}

static inline uint32_t bytestream_get_be32(const uint8_t **ptr) {
    uint32_t tmp;
    tmp = (*ptr)[3] | ((*ptr)[2]<<8) | ((*ptr)[1]<<16) | ((*ptr)[0]<<24);
    *ptr += 4;
    return tmp;
}
static inline uint32_t bytestream_get_be24(const uint8_t **ptr) {
    uint32_t tmp;
    tmp = (*ptr)[2] | ((*ptr)[1]<<8) | ((*ptr)[0]<<16);
    *ptr += 3;
    return tmp;
}
static inline uint32_t bytestream_get_be16(const uint8_t **ptr) {
    uint32_t tmp;
    tmp = (*ptr)[1] | ((*ptr)[0]<<8);
    *ptr += 2;
    return tmp;
}
static inline uint8_t bytestream_get_byte(const uint8_t **ptr) {
    uint8_t tmp;
    tmp = **ptr;
    *ptr += 1;
    return tmp;
}

/**
 *  return the literal value of ascii printed char
 */
static inline int subAscii2Value(char ascii) {
    return ascii - '0';
}

/**
 *  Peek the buffer data, consider it to int 32 value.
 *
 *  Peek, do not affect the buffer contents and pointer
 */
static inline int32_t subPeekAsInt32(const char* buffer) {
    int32_t value = 0;
    for (auto i = 0; i < 4; ++i) {
        value <<= 8;
        value |= buffer[i];
    }
    return value;
}

/**
 *  Peek the buffer data, consider it to unsigned int 32 value.
 *
 *  Peek, do not affect the buffer contents and pointer
 */
static inline uint32_t subPeekAsUint32(const char* buffer) {
    uint32_t value = 0;
    for (auto i = 0; i < 4; ++i) {
        value <<= 8;
        value |= buffer[i];
    }
    return value;
}

/**
 *  Peek the buffer data, consider it to int 64 value.
 *
 *  Peek, do not affect the buffer contents and pointer
 */
static inline int64_t subPeekAsInt64(const char* buffer) {
    int64_t value = 0;
    for (auto i = 0; i < 8; ++i) {
        value <<= 8;
        value |= buffer[i];
    }
    return value;
}

/**
 *  Peek the buffer data, consider it to unsigned int 64 value.
 *
 *  Peek, do not affect the buffer contents and pointer
 */
static inline uint64_t subPeekAsUint64(const char* buffer) {
    uint64_t value = 0;
    for (auto i = 0; i < 8; ++i) {
        value <<= 8;
        value |= buffer[i];
    }
    return value;
}

