#include <sys/syscall.h>

// hint: -Wl,--entry=__start

#define NONE 0

// syscall de-wrappers
#if defined(__aarch64__)
__attribute__((noinline))
static long __syscall(long n, long a, long b, long c, long d, long e, long f)
{
	register long 
		x8 asm("x8") = n,
		x0 asm("x0") = a,
		x1 asm("x1") = b,
		x2 asm("x2") = c,
		x3 asm("x3") = d,
		x4 asm("x4") = e,
		x5 asm("x5") = f;

	asm volatile("svc #0"
		:"=r"(x0)
		:"r"(x8), "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
		:"memory");

	return x0;
}

#elif defined(__arm__)
__attribute__((noinline))
static long __syscall(long n, long a, long b, long c, long d, long e, long f) {
	register long
		r7 asm("r7") = n,
		r0 asm("r0") = a,
		r1 asm("r1") = b,
		r2 asm("r2") = c,
		r3 asm("r3") = d,
		r4 asm("r4") = e,
		r5 asm("r5") = f;

	asm volatile("svc #0"
		: "=r"(r0)
		: "r"(r7), "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
		: "memory");

	return r0;
}

#elif defined(__x86_64__)
__attribute__((noinline))
static long __syscall(long n, long a, long b, long c, long d, long e, long f) {
	long ret;
	asm volatile(
		"mov %5, %%r10\n"
		"mov %6, %%r8\n"
		"mov %7, %%r9\n"
		"syscall"
		: "=a"(ret)
		: "a"(n), "D"(a), "S"(b), "d"(c), "r"(d), "r"(e), "r"(f)
		: "rcx", "r11", 
		"memory");

	return ret;
}
#endif

#if 0 // this is here so YOU can copy-paste below C entry
void prep_main(long *sp)
{
	long argc = *sp;
	char **argv = (char **)(sp + 1);
	char **envp = argv + argc + 1; // we need to offset it by the number of argc's!

	long exit_code = c_main(argc, argv, envp);
	__syscall(SYS_exit, exit_code, NONE, NONE, NONE, NONE, NONE);
}
#endif

// arch specific small entry points
#if defined(__aarch64__)
__attribute__((naked))
void __start(void) {
	asm volatile(
		"mov x0, sp\n"
		"b prep_main\n"
	);
}

#elif defined(__arm__)
__attribute__((naked))
void __start(void) {
	asm volatile(
		"mov r0, sp\n"
		"b prep_main\n"
    );
}

#elif defined(__x86_64__)
__attribute__((naked, section(".text.start"))) 
void __start(void) {
	asm volatile(
		"mov %rsp, %rdi\n"
		"jmp prep_main\n"
	);
}
#endif

