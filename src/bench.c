#include <linux/utsname.h>

#define N_ITERATIONS 1000000
#define N_ITERATIONS_DIGITS 7

const char extra_lines[] = 
	"[!] tests:\n"
	"[1] NULL\n"
	"[2] /dev/null\n"
	"[3] /system/bin/su_\n"
	"[4] *unaligned*\n"
	"[*] Lower is better, * = untracked\n";

const char *devnull = "/dev/null";

const char run_template[] = "[+] kernel: ";
const char iter_template[] = "[+] iterations: ";
char cpu_core_template[] = " | core: ??\n";
char newline[] = "\n";
char result_template[] = "(0000000 ns avg)\n";
char box_template[] = "[ ] ";
char sucompat_seccomp_root_template[] = "[+] sucompat: 0 | seccomp: ? | root: ";

uint64_t total_avg = 0;
char total_avg_template[] = "[+] total avgs:   000000000\n";

cpu_set_t cpuset;

#if defined(__aarch64__)
static long payload_faccessat2() {	
	return __syscall(SYS_faccessat2, AT_FDCWD, (long)devnull, F_OK, 0, NONE, NONE);
}

__attribute__((always_inline))
static bool run_forked_payload(long (*payload_fn)())
{
	long pid = __syscall(SYS_clone, SIGCHLD, NULL, NULL, NULL, NULL, NULL);
	if (pid == -1)
		return false;

	if (!!pid)
		goto main_thread;
	
	long ret = payload_fn();
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
#endif

/**
 * NOTE: this might be actually slower now as this forces a syscall
 * clock_gettime by default is routed through vDSO.
 * but I think this is fair game as we are benchmarking syscalls
 */
 
#if defined(__arm__) 

#define SYS_clock_gettime32 263

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
static void run_bench(long sc, long a1, long a2, long a3, long a4, long a5, long a6, bool track, char *template)
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

	if (track)
		total_avg = total_avg + ((t1 - t0) / N_ITERATIONS);
}

__attribute__((always_inline))
static int get_highest_cpu_core()
{
	__syscall(SYS_sched_getaffinity, 0, sizeof(cpuset), &cpuset, NONE, NONE, NONE);

	// we dont really have much cores on our targets, so first member is enough. assumes LE for core 0~31!
	uint32_t lowmask = *(uint32_t __attribute__((may_alias)) *)&cpuset;

	int top_cpu = 0;
	if (lowmask)
	    top_cpu = 31 - __builtin_clz(lowmask); // popcount can also be used, however, clz is smaller.

	return top_cpu;
}

__attribute__((always_inline))
static bool affine_to_cpu(int cpu)
{
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	return !!!__syscall(SYS_sched_setaffinity, 0, sizeof(cpuset), &cpuset, NONE, NONE, NONE);
}

__attribute__((always_inline))
static int bench_main(char **argv)
{
#if defined(__aarch64__) // check extra access syscalls, SYS_faccessat2 (aarch64)
	bool has_access_sc = run_forked_payload(payload_faccessat2);
#endif

#define PR_GET_SECCOMP	21
	int seccomp_status = __syscall(SYS_prctl, PR_GET_SECCOMP, NONE, NONE, NONE, NONE, NONE);

	bool is_root = !!!__syscall(SYS_getuid, NONE, NONE, NONE, NONE, NONE, NONE);

	int pinned_cpu_core;
	if (argv[2])
		pinned_cpu_core = dumb_atoi(argv[2]); // lets just accept anything user says
	else 
		pinned_cpu_core = get_highest_cpu_core();

	affine_to_cpu(pinned_cpu_core);

	__syscall(SYS_setpriority, 0, 0, -20, NONE, NONE, NONE);

	struct stat st;

	struct new_utsname uname;
	__syscall(SYS_uname, (long)&uname, NONE, NONE, NONE, NONE, NONE); // SYS_newuname on syscall table

	// maybe writev
	print_out(run_template, sizeof(run_template) - 1);
	print_out(uname.release, strlen(uname.release));
	print_out(newline, sizeof(newline) - 1 );

	char iter_buf[N_ITERATIONS_DIGITS];
	dumb_itoa(N_ITERATIONS, N_ITERATIONS_DIGITS, iter_buf);
	print_out(iter_template, sizeof(iter_template) - 1);
	print_out(iter_buf, N_ITERATIONS_DIGITS);

	dumb_itoa(pinned_cpu_core, 2, cpu_core_template + 9);
	print_out(cpu_core_template, sizeof(cpu_core_template) -1 );

	if (!__syscall(SYS_faccessat, AT_FDCWD, (long)"/system/bin/su", F_OK, NONE, NONE, NONE))
		sucompat_seccomp_root_template[14] = '1';

	if (seccomp_status == 0)
		sucompat_seccomp_root_template[27] = '0';
	if (seccomp_status == 1)
		sucompat_seccomp_root_template[27] = '1';
	if (seccomp_status == 2)
		sucompat_seccomp_root_template[27] = '2';

	print_out(sucompat_seccomp_root_template, sizeof(sucompat_seccomp_root_template) - 1);

	const char *str_yes_no = "yes\nno\n";
	if (is_root)
		print_out(str_yes_no, 4 );
	else
		print_out(str_yes_no + 4, 3);

	const void *nothing = nullptr;
	const char *notsu = "/system/bin/su_";
	const char *unaligned = notsu + 3;

	print_out(extra_lines, sizeof(extra_lines) - 1 );

	print_out(newline, sizeof(newline) - 1 );

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

#if defined(__arm__) 
#define SYS_newfstatat SYS_fstatat64
#endif
	run_bench(SYS_execve, (long)tests[j], NULL, NULL, NONE, NONE, NONE, true, "execve:      ");
	run_bench(SYS_newfstatat, AT_FDCWD, (long)tests[j], (long)&st, AT_SYMLINK_NOFOLLOW, NONE, NONE, true, "newfstatat:  ");
	run_bench(SYS_faccessat, AT_FDCWD, (long)tests[j], F_OK, NONE, NONE, NONE, true, "faccessat:   ");

#if defined(__aarch64__)
	if (has_access_sc)
		run_bench(SYS_faccessat2, AT_FDCWD, (long)tests[j], F_OK, 0, NONE, NONE, false, "faccessat2*: ");
#else
	run_bench(SYS_access, (long)tests[j], F_OK, NONE, NONE, NONE, NONE, false, "access*:     ");
#endif
	
	j++;
	
	if (j < num_tests)
		goto start_loop;

	print_out(newline, 1);

	dumb_itoa(total_avg, 9, total_avg_template + 18);
	print_out(total_avg_template, sizeof(total_avg_template) - 1);

	return 0;
}
