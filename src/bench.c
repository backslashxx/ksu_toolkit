#include <linux/utsname.h>

#define N_ITERATIONS 1000000
#define N_ITERATIONS_DIGITS 7

const char extra_lines[] = 
	"[!] tests:\n"
	"[1] NULL\n"
	"[2] /dev/null\n"
	"[3] /system/bin/su_\n"
	"[4] *unaligned*\n"
	"[*] Lower is better\n";

const char run_template[] = "[+] kernel: ";
const char iter_template[] = "[+] iterations: ";
char newline[] = "\n";
char result_template[] = "(0000000 ns avg)\n";
char box_template[] = "[ ] ";
char sucompat_seccomp_template[] = "[+] sucompat: ? | seccomp: ? \n";

__attribute__((always_inline))
static bool check_seccomp() {
	long pid = __syscall(SYS_clone, SIGCHLD, NULL, NULL, NULL, NULL, NULL);
	if (pid == -1)
		return false;

	if (pid == 0) {
		__syscall(SYS_swapoff, NULL, NULL, NULL, NULL, NULL, NULL);
		__syscall(SYS_exit, 0, NULL, NULL, NULL, NULL, NULL);
		__builtin_unreachable();
	}

	int status = 0;
	__syscall(SYS_wait4, pid, &status, 0, NULL, NULL, NULL);
	if (WIFSIGNALED(status))
		return true; // means it died weirdly
	
	return false;
}

__attribute__((always_inline))
static bool is_access_syscall_ok()
{
	long pid = __syscall(SYS_clone, SIGCHLD, NULL, NULL, NULL, NULL, NULL);
	if (pid == -1)
		return false;

	const char *devnull = "/dev/null";

	if (pid != 0)
		goto main_thread;

	// child
	long ret = __syscall(SYS_faccessat2, AT_FDCWD, (long)devnull, F_OK, 0, NONE, NONE);
	if (ret == -ENOSYS)
		__syscall(SYS_exit, 123, NONE, NONE, NONE, NONE, NONE);
	else
		__syscall(SYS_exit, 0, NONE, NONE, NONE, NONE, NONE);

	__builtin_unreachable();

main_thread:

	int status = 0;
	__syscall(SYS_wait4, pid, &status, 0, NULL, NULL, NULL);
	if (WIFSIGNALED(status))
		return false; // means it died weirdly

	if (WIFEXITED(status) && WEXITSTATUS(status) == 123)
		return false;

	return true;
}

/**
 * NOTE: this might be actually slower now as this forces a syscall
 * clock_gettime by default is routed through vDSO.
 * but I think this is fair game as we are benchmarking syscalls
 */
 
#if defined(__arm__) 

#define SYS_newfstatat SYS_fstatat64
#define SYS_clock_gettime32 263
#define SYS_setresuid16 164

struct old_timespec32 {
	int32_t	tv_sec;
	int32_t	tv_nsec;
};

__attribute__((always_inline))
static unsigned long long time_now_ns() {
	struct old_timespec32 ts32;
	long clk_id;

#ifdef CLOCK_MONOTONIC_RAW
	clk_id = CLOCK_MONOTONIC_RAW;
#else
	clk_id = CLOCK_MONOTONIC;
#endif
	__syscall(SYS_clock_gettime32, clk_id, (long)&ts32, NONE, NONE, NONE, NONE);

	return (unsigned long long)ts32.tv_sec * 1000000000ULL + ts32.tv_nsec;
}

#else /* ! arm */

__attribute__((always_inline))
static unsigned long long time_now_ns() {
	struct timespec ts;
	long clk_id;

#ifdef CLOCK_MONOTONIC_RAW
	clk_id = CLOCK_MONOTONIC_RAW;
#else
	clk_id = CLOCK_MONOTONIC;
#endif
	__syscall(SYS_clock_gettime, clk_id, (long)&ts, NONE, NONE, NONE, NONE);
	return (unsigned long long)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#endif // __arm__

__attribute__((noinline))
static void run_bench(long sc, long a1, long a2, long a3, long a4, long a5, long a6, char *template)
{
	uint64_t t0, t1;
	long i = 0;

	t0 = time_now_ns();
bench_start:
	__syscall(sc, a1, a2, a3, a4, a5, a6);
	i++;
	if (i < N_ITERATIONS)
		goto bench_start;

	t1 = time_now_ns();
	print_out(box_template, sizeof(box_template) - 1 );
	print_out(template, strlen(template));
	dumb_itoa((t1 - t0) / N_ITERATIONS, 7, result_template + 1);
	print_out(result_template, sizeof(result_template) -1 );
}

__attribute__((always_inline))
static int bench_main()
{
	bool is_seccomp_enabled = check_seccomp();

	// try to pin core 7, this normally is within the "big" cluster
	cpu_set_t cpuset = { 0 };
	CPU_SET(7, &cpuset);
	if (!__syscall(SYS_sched_setaffinity, 0, sizeof(cpuset), &cpuset, NONE, NONE, NONE))
		goto setpriority;
	
	// for 4 + 2 chips like sd808
	CPU_ZERO(&cpuset);
	CPU_SET(5, &cpuset);
	if (!__syscall(SYS_sched_setaffinity, 0, sizeof(cpuset), &cpuset, NONE, NONE, NONE))
		goto setpriority;

	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	// fallback to core 0, all devices have this duh
	__syscall(SYS_sched_setaffinity, 0, sizeof(cpuset), &cpuset, NONE, NONE, NONE);

setpriority:
	__syscall(SYS_setpriority, 0, 0, -20, NONE, NONE, NONE);

	struct stat st;

	struct new_utsname uname;
	__syscall(SYS_uname, (long)&uname, NONE, NONE, NONE, NONE, NONE); // SYS_newuname on syscall table

	// maybe writev
	print_out(run_template, sizeof(run_template) - 1);
	print_out(uname.release, strlen(uname.release));
	print_out(newline, sizeof(newline) - 1 );

	if (!__syscall(SYS_faccessat, AT_FDCWD, (long)"/system/bin/su", F_OK, NONE, NONE, NONE))
		sucompat_seccomp_template[14] = 49;
	else
		sucompat_seccomp_template[14] = 48;

	if (is_seccomp_enabled)
		sucompat_seccomp_template[27] = 49;
	else
		sucompat_seccomp_template[27] = 48;

	print_out(sucompat_seccomp_template, sizeof(sucompat_seccomp_template) - 1 );

	char iter_buf[N_ITERATIONS_DIGITS];
	dumb_itoa(N_ITERATIONS, N_ITERATIONS_DIGITS, iter_buf);
	print_out(iter_template, sizeof(iter_template) - 1);
	print_out(iter_buf, N_ITERATIONS_DIGITS);
	print_out(newline, sizeof(newline) - 1 );

	const void *nothing = nullptr;
	const char *devnull = "/dev/null";
	const char *notsu = "/system/bin/su_";
	const char *unaligned = notsu + 3;

	print_out(extra_lines, sizeof(extra_lines) - 1 );

	print_out(newline, sizeof(newline) - 1 );

	bool is_root = !!!__syscall(SYS_getuid, NONE, NONE, NONE, NONE, NONE, NONE);

	// check extra access syscalls, SYS_faccessat2 (aarch64)
	bool has_access_sc = is_access_syscall_ok();

	if (is_root)
		goto skip_setresuid;

#if defined(__arm__) 
	run_bench(SYS_setresuid16, 10000, 10000, 10000, NONE, NONE, NONE, "setresuid16: ");
#endif

	run_bench(SYS_setresuid, 10000, 10000, 10000, NONE, NONE, NONE, "setresuid:   ");
	print_out(newline, sizeof(newline) - 1 );

skip_setresuid:
	const void *tests[] = {
		nothing,
		devnull,
		notsu,
		unaligned
	};

	const int num_tests = sizeof(tests) / sizeof(tests[0]);

	int j = 0;

start_loop:
	box_template[1] = 49 + j; // off by one, array starts with 0, humans count with 1

	run_bench(SYS_execve, (long)tests[j], NULL, NULL, NONE, NONE, NONE, "execve:      ");
	run_bench(SYS_newfstatat, AT_FDCWD, (long)tests[j], (long)&st, AT_SYMLINK_NOFOLLOW, NONE, NONE, "newfstatat:  ");
	run_bench(SYS_faccessat, AT_FDCWD, (long)tests[j], F_OK, NONE, NONE, NONE, "faccessat:   ");

	if (has_access_sc)
		run_bench(SYS_faccessat2, AT_FDCWD, (long)tests[j], F_OK, 0, NONE, NONE, "faccessat2:  ");


	print_out(newline, 1);
	
	j++;
	
	if (j < num_tests)
		goto start_loop;

	return 0;
}
