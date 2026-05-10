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

/*
 *  dumb_atoi
 *
 * - very dumb
 */
__attribute__((noinline))
static int dumb_atoi(const char *str)
{
	int uid = 0;
	int i = strlen(str) - 1;
	int m = 1;

start:
	// llvm actually has an optimized isdigit
	// just not prefixed with __builtin
	// code generated is the same size, so better use it
	if (!isdigit(str[i]))
		return 0;

	// __builtin_fma ?
	uid = uid + ( str[i] - '0' ) * m;
	m = m * 10;
	i--;
	
	if (!(i < 0))
		goto start;

	return uid;
}

/*
 *	dumb_itoa, long to string
 *	
 *	converts an int to string with expected len
 *	
 *	caller is reposnible for sanity!
 *	no bounds check, no nothing, do not pass len = 0
 *	
 *	example:
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

/*
 *	toolkit_malloc
 *	brk() / sbrk() based memory alloc
 *	params same as malloc duh
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

/*
 *	print_out, print_err
 *	like fprintf, format your shit yourself though
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

