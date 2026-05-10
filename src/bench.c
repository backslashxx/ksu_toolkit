#define N_ITERATIONS 1000000
#define N_ITERATIONS_DIGITS 7

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

	// CLOCK_MONOTONIC is 1
	__syscall(SYS_clock_gettime32, 1, (long)&ts32, NONE, NONE, NONE, NONE);

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

char result_template[] = "(0000000 ns avg)\n";
char box_template[] = "[ ] ";

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

	// Pin to core 0 for consistency
	cpu_set_t cpuset = {};
	CPU_SET(0, &cpuset);
	__syscall(SYS_sched_setaffinity, 0, sizeof(cpuset), &cpuset, NULL, NULL, NULL);
	__syscall(SYS_setpriority, 0, 0, -20, NONE, NONE, NONE);

	struct stat st;

	const char run_template[] = "[+] run ";
	char iter_buf[N_ITERATIONS_DIGITS];
	const char iter_template[] = " iterations per syscall...\n";

	print_out(run_template, sizeof(run_template) - 1);
	dumb_itoa(N_ITERATIONS, N_ITERATIONS_DIGITS, iter_buf);
	print_out(iter_buf, N_ITERATIONS_DIGITS);
	print_out(iter_template, sizeof(iter_template) - 1);

	const char su_found[] = "[+] /system/bin/su found! sucompat is active.\n";
	const char su_not_found[] = "[-] /system/bin/su not found! sucompat is disabled.\n";
	const char seccomp_enabled[] = "[+] seccomp enabled\n";
	const char seccomp_disabled[] = "[-] seccomp disabled\n";
	const char extra_lines[] = 
		"[!] note:\n"
		"[1] NULL\n"
		"[2] /dev/null\n"
		"[3] /system/bin/su_\n"
		"[4] *unaligned*\n"
		"[*] Lower is better\n";

	char newline[] = "\n";

	if (!__syscall(SYS_faccessat, AT_FDCWD, (long)"/system/bin/su", F_OK, NONE, NONE, NONE))
        	print_out(su_found, sizeof(su_found) - 1 );
        else
        	print_out(su_not_found, sizeof(su_not_found) - 1 );

	if (is_seccomp_enabled)
        	print_out(seccomp_enabled, sizeof(seccomp_enabled) - 1 );
	else
        	print_out(seccomp_disabled, sizeof(seccomp_disabled) - 1 );

	print_out(extra_lines, sizeof(extra_lines) - 1 );

	const void *nothing = nullptr;
	const char *devnull = "/dev/null";
	const char *notsu = "/system/bin/su_";
	const char *unaligned = notsu + 3;

	print_out(newline, sizeof(newline) - 1 );

#if defined(__arm__) 
	run_bench(SYS_setresuid16, 10000, 10000, 10000, NULL, NULL, NULL, "setresuid16: ");
#endif

	run_bench(SYS_setresuid, 10000, 10000, 10000, NULL, NULL, NULL, "setresuid:   ");
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
    
	run_bench(SYS_newfstatat, AT_FDCWD, (long)tests[j], (long)&st, NULL, NULL, NULL, "newfstatat:  ");
	run_bench(SYS_faccessat, AT_FDCWD, (long)tests[j], F_OK, NULL, NULL, NULL, "faccessat:   ");
	run_bench(SYS_execve, (long)tests[j], NULL, NULL, NULL, NULL, NULL, "execve:      ");
    
	print_out(newline, 1);
	
	j++;
	
	if (j < num_tests)
		goto start_loop;

	return 0;
}
