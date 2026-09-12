#pragma once

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <sched.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <ctype.h>
#include <stddef.h>

// builtins
#define memcmp __builtin_memcmp

__attribute__((noinline))
static unsigned long strlen(const char *str)
{
	const char *s = str;
	while (*s)
		s++;

	return s - str;
}

/**
 * dumb_atoi, string to int
 *
 * converts a string to int
 * assumes numeric char
 *
 * caller is responsible for sanity
 *
 */
__attribute__((noinline))
static int dumb_atoi(const char *str)
{
	int res = 0;

start:
	// llvm actually has an optimized isdigit
	// just not prefixed with __builtin
	// code generated is the same size, so better use it
	if (!isdigit(*str))
		return 0;

	res = (res * 10) + (*str - 48);
	str++;

	if (*str)
		goto start;

	return res;
}

/**
 * dumb_itoa, long to string
 *	
 * converts an int to string with expected len
 *	
 * caller is reposnible for sanity!
 * no bounds check, no nothing, do not pass len = 0
 *	
 * example:
 *	long_to_str(10123, 5, buf); // where buf is char buf[5]; atleast
 */
__attribute__((noinline))
static void dumb_itoa(unsigned long number, unsigned long len, char *buf)
{

start:
	buf[len - 1] = 48 + (number % 10);
	number = number / 10;
	len--;

	if (len > 0)
		goto start;

	return;
}

/**
 * toolkit_malloc
 * brk() / sbrk() based memory alloc
 * params same as malloc duh
 *
 */
__attribute__((always_inline))
static void *toolkit_malloc(unsigned long size)
{
	size = (size + 7) & ~7; // align 8 bytes up

	long current_brk = __syscall(SYS_brk, 0, NONE, NONE, NONE, NONE, NONE);

	long new_brk = current_brk + size;
	long ret = __syscall(SYS_brk, new_brk, NONE, NONE, NONE, NONE, NONE);
	if (ret != new_brk)
		return nullptr;

	return (void *)current_brk;
}

/**
 * print_out, print_err
 * like fprintf, format your shit yourself though
 *
 */
__attribute__((noinline))
static void print_out(const char *buf, unsigned long len)
{
	__syscall(SYS_write, 1, (long)buf, len, NONE, NONE, NONE);
}

__attribute__((noinline))
static void print_err(const char *buf, unsigned long len)
{
	__syscall(SYS_write, 2, (long)buf, len, NONE, NONE, NONE);
}

#ifndef __has_builtin
#define __has_builtin(x) (0)
#endif

#ifndef __has_feature
#define __has_feature(x) (0)
#endif

#ifndef __has_c_attribute
#define __has_c_attribute(x) (0)
#endif

#ifndef __has_include
#define __has_include(x) (0)
#endif

#ifndef __has_extension
#define __has_extension(x) (0)
#endif

#ifndef __has_attribute
#define __has_attribute(x) (0)
#endif

#if (defined(__clang__) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L) || \
	(!defined(__clang__) && (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L))
#define COMPILER_HAS_C23
#endif

/**
 * partially emulate-able C23 features, should be fine on GNU11 compilers
 *
 * Limitations:
 *	- do NOT use nullptr_t on _Generic overloading, it will fuck up on C11
 *	- do NOT use constexpr as array size on C11, it will likely become a VLA
 */
#ifndef COMPILER_HAS_C23
#define nullptr ((void *)0)
typedef typeof(nullptr) nullptr_t;
#define constexpr const
#define auto __auto_type
#define alignas _Alignas
#define alignof _Alignof
// note: requires clang
// #define typeof_unqual(a) typeof(0, (a))
#endif // COMPILER_HAS_C23

// NOTE: clang < 19 has issues on constexpr even with -std=gnu23
#if defined(COMPILER_HAS_C23) && defined(__clang__) && (__clang_major__ < 19)
#define constexpr const
#endif

/**
 * C2y's countof
 */
#if __has_feature(c_countof) || __has_extension(c_countof)
#define countof(a) _Countof(a)
#else
#define countof(a) (sizeof(a) / sizeof(a[0]))
#endif
