/* SPDX-License-Identifier: MIT */
#pragma once
/* Upstream tinf is zlib-licensed; see third_party/tinf/LICENSE. */
#include "third_party/tinf/tinflate.c"
static int art_unpack(uint8_t *dst, uint32_t size, const uint8_t *src, uint32_t bytes)
{
    unsigned int written = size;
    return tinf_uncompress(dst, &written, src, bytes) == TINF_OK && written == size;
}
