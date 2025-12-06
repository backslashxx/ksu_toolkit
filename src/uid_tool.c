#include <stdint.h>
#include <sys/ioctl.h>
#include <ctype.h>

#include "small_rt.h"

// zig cc -target aarch64-linux -Oz -s -Wl,--gc-sections,--strip-all,-z,norelro -fno-unwind-tables -Wl,--entry=__start -Wno-implicit-function-declaration uid_tool.c -o uid_tool 

// https://gcc.gnu.org/onlinedocs/gcc/Library-Builtins.html
// https://clang.llvm.org/docs/LanguageExtensions.html#builtin-functions
#define alloca __builtin_alloca
#define memcmp __builtin_memcmp
#define strlen __builtin_strlen

// get uid from kernelsu
struct ksu_get_manager_uid_cmd {
	uint32_t uid;
};
#define KSU_IOCTL_GET_MANAGER_UID _IOC(_IOC_READ, 'K', 10, 0)

struct ksu_add_try_umount_cmd {
	uint64_t arg; // char ptr, this is the mountpoint
	uint32_t flags; // this is the flag we use for it
	uint8_t mode; // denotes what to do with it 0:wipe_list 1:add_to_list 2:delete_entry
};
#define KSU_UMOUNT_GETSIZE 107   // get list size
#define KSU_UMOUNT_GETLIST 108   // get list

#define KSU_IOCTL_ADD_TRY_UMOUNT _IOC(_IOC_WRITE, 'K', 18, 0)

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

__attribute__((noinline))
static int fail(const char *error)
{
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

static int c_main(int argc, char **argv, char **envp)
{
	const char *ok = "ok\n";
	const char *newline = "\n";
	const char usage[] =
	"Usage:\n"
	"./toolkit --setuid <uid>\n"
	"./toolkit --getuid\n"
	"./toolkit --getlist\n";

	unsigned int fd = 0;

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

	if (!memcmp(argv[1], "--getlist", strlen("--getlist") + 1) && !argv[2]) {
		unsigned long total_size = 0;

		__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, (long)&fd, NONE, NONE);
		if (!fd)
			return 1;

		struct ksu_add_try_umount_cmd cmd = {0};
		cmd.arg = (uint64_t)&total_size;
		cmd.flags = 0;
		cmd.mode = KSU_UMOUNT_GETSIZE;


		int ret = __syscall(SYS_ioctl, fd, KSU_IOCTL_ADD_TRY_UMOUNT, (long)&cmd, NONE, NONE, NONE);
		if (ret < 0)
			goto fail;

		if (!total_size)
			goto list_empty;

		// now we can prepare the same size of memory		
		void *buffer = alloca(total_size);
		if (!buffer)
			goto fail;

		cmd.arg = (uint64_t)buffer;
		cmd.flags = 0;
		cmd.mode = KSU_UMOUNT_GETLIST;

		ret = __syscall(SYS_ioctl, fd, KSU_IOCTL_ADD_TRY_UMOUNT, (long)&cmd, NONE, NONE, NONE);
		if (ret < 0)
			goto fail;

		// now we pointerwalk
		const char *char_buf = (const char *)buffer;
		do {
			__syscall(SYS_write, 1, (long)char_buf, strlen(char_buf), NONE, NONE, NONE);
			__syscall(SYS_write, 1, (long)newline, 1, NONE, NONE, NONE);
			
			char_buf = char_buf + strlen(char_buf) + 1;
		} while (*char_buf);

		return 0;
	}

show_usage:
	return fail(usage);

list_empty:
	return fail("list empty\n");

fail:
	return fail("fail\n");
}

void prep_main(long *sp)
{
	long argc = *sp;
	char **argv = (char **)(sp + 1);
	char **envp = (char **)(sp + 2);

	long exit_code = c_main(argc, argv, envp);
	__syscall(SYS_exit, exit_code, NONE, NONE, NONE, NONE, NONE);
}
