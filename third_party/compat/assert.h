/* SPDX-License-Identifier: MIT */
/* Freestanding target, equivalent to standard assert.h with NDEBUG.
 * The host build uses libc assert.h and keeps tinf's internal assertions. */
#pragma once
#define assert(expression) ((void)0)
