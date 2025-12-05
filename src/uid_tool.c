#include <stdint.h>
#include <sys/ioctl.h>
#include <ctype.h>

#include "small_rt.h"

// zig cc -target aarch64-linux -Oz -s -Wl,--gc-sections,--strip-all,-z,norelro -fno-unwind-tables -Wl,--entry=__start -Wno-implicit-function-declaration uid_tool.c -o uid_tool 

// https://gcc.gnu.org/onlinedocs/gcc/Library-Builtins.html
// https://clang.llvm.org/docs/LanguageExtensions.html#builtin-functions
#define strlen __builtin_strlen
#define memcmp __builtin_memcmp

// get uid from kernelsu
struct ksu_get_manager_uid_cmd {
	uint32_t uid;
};
#define KSU_IOCTL_GET_MANAGER_UID _IOC(_IOC_READ, 'K', 10, 0)
#define KSU_INSTALL_MAGIC1 0xDEADBEEF
#define KSU_INSTALL_MAGIC2 0xCAFEBABE

#define NONE 0

__attribute__((always_inline))
static int dumb_str_to_appuid(const char *str)
{
	int uid = 0;
	int i = 4;
	int m = 1;

	do {
		// llvm actually has an optimized isdigit
		// just not prefixed with __builtin
		// code generated is the same size, so better use it
		if (!isdigit(str[i]))
			return 0;

		uid = uid + ( *(str + i) - 48 ) * m;
		m = m * 10;
		i--;
	} while (!(i < 0));

	if (!(uid > 10000 && uid < 20000))
		return 0;

	return uid;
}

__attribute__((always_inline))
static int fail(void)
{
	const char *error = "fail\n";
	__syscall(SYS_write, 2, (long)error, strlen(error), NONE, NONE, NONE);
	return 1;
}

// https://github.com/backslashxx/various_stuff/blob/master/ksu_prctl_test/ksu_prctl_02_only.c
__attribute__((always_inline))
static int dumb_print_appuid(int uid)
{
	char digits[6];

	int i = 4;
	do {
		digits[i] = 48 + (uid % 10);
		uid = uid / 10;
		i--;			
	} while (!(i < 0));

	digits[5] = '\n';

	__syscall(SYS_write, 1, (long)digits, 6, NONE, NONE, NONE);
	return 0;
}

__attribute__((always_inline))
static int show_usage(void)
{
	const char *usage = "Usage:\n./uidtool --setuid <uid>\n./uidtool --getuid\n";
	__syscall(SYS_write, 2, (long)usage, strlen(usage), NONE, NONE, NONE);
	return 1;
}

static int c_main(int argc, char **argv, char **envp)
{
	const char *ok = "ok\n";

	if (!argv[1])
		goto show_usage;

	if (!memcmp(argv[1], "--setuid", strlen("--setuid") + 1) && 
		!!argv[2] && !!argv[2][4] && !argv[2][5] && !argv[3]) {
		int magic1 = 0xDEADBEEF;
		int magic2 = 10006;
		uintptr_t arg = 0;
		
		unsigned int cmd = dumb_str_to_appuid(argv[2]);
		if (!cmd)
			goto fail;
		
		__syscall(SYS_reboot, magic1, magic2, cmd, (long)&arg, NONE, NONE);

		if (arg && *(uintptr_t *)arg == arg ) {
			__syscall(SYS_write, 2, (long)ok, strlen(ok), NONE, NONE, NONE);
			return 0;
		}
		
		goto fail;
	}

	if (!memcmp(argv[1], "--getuid", strlen("--getuid") + 1) && !argv[2]) {
		unsigned int fd = 0;
		
		// we dont care about closing the fd, it gets released on exit automatically
		__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, (long)&fd, NONE, NONE);
		if (!fd)
			goto fail;

		struct ksu_get_manager_uid_cmd cmd;
		int ret = __syscall(SYS_ioctl, fd, KSU_IOCTL_GET_MANAGER_UID, (long)&cmd, NONE, NONE, NONE);
		if (ret)
			goto fail;

		if (!(cmd.uid > 10000 && cmd.uid < 20000))
			goto fail;

		return dumb_print_appuid(cmd.uid);
	}

show_usage:
	return show_usage();

fail:
	return fail();
}

void prep_main(long *sp)
{
	long argc = *sp;
	char **argv = (char **)(sp + 1);
	char **envp = (char **)(sp + 2);

	long exit_code = c_main(argc, argv, envp);
	__syscall(SYS_exit, exit_code, NONE, NONE, NONE, NONE, NONE);
}
