// This file is part of the Sphynx OS
// It is released under the MIT license -- see LICENSE
// Written by: Kevin Alavik.

#pragma once

#include <stdarg.h>
#include <lib/std/lock.h>

extern Spinlock vprintf_lock;
extern Spinlock vdprintf_lock;

void TTYInitialize();

void vprintf(const char *fmt, va_list args);
void vdprintf(const char *fmt, va_list args);

void putchar(char ch, int i);
