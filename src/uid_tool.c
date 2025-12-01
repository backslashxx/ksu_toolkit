#include <sys/syscall.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>

// zig cc -target aarch64-linux uid_tool.c -Oz -s -Wl,--gc-sections,--strip-all,-z,norelro -fno-unwind-tables -flto -o uid_tool

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

__attribute__((always_inline))
static int dumb_str_to_appuid(const char *str)
{
	int uid = 0;

	// dereference to see if user supplied 5 digits
	if ( !*(str + 4) )
		return 0;
	
	// dereference to see if its a null terminator
	if ( *(str + 5) )
		return 0;

	int i = 4;
	int m = 1;

	do {
		// like what? you'll put a letter? a symbol?
		if ( *(str + i ) > '9' || *(str + i ) < '0' )
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
	syscall(SYS_write, 2, error, strlen(error));
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

	syscall(SYS_write, 1, digits, 6);
	return 0;
}

__attribute__((always_inline))
static int show_usage(void)
{
	const char *usage = "Usage:\n./uidtool --setuid <uid>\n./uidtool --getuid\n";
	syscall(SYS_write, 2, usage, strlen(usage));
	return 1;
}

int main(int argc, char *argv[])
{
	if (!argv[1])
		goto show_usage;

	if (!!argv[2] && !argv[3] && !memcmp(argv[1], "--setuid", strlen("--setuid") + 1)) {
		int magic1 = 0xDEADBEEF;
		int magic2 = 10006;
		uintptr_t arg = 0;
		
		unsigned int cmd = dumb_str_to_appuid(argv[2]);
		if (!cmd)
			goto fail;
		
		syscall(SYS_reboot, magic1, magic2, cmd, (void *)&arg);

		if (arg && *(uintptr_t *)arg == arg ) {
			syscall(SYS_write, 2, "ok\n", strlen("ok\n"));
			return 0;
		}
		
		goto fail;
	}

	if (!argv[2] && !memcmp(argv[1], "--getuid", strlen("--getuid") + 1)) {
		unsigned int fd = 0;
		
		// we dont care about closing the fd, it gets released on exit automatically
		syscall(SYS_reboot, KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, (void *)&fd);
		if (!fd)
			goto fail;

		struct ksu_get_manager_uid_cmd cmd;
		int ret = syscall(SYS_ioctl, fd, KSU_IOCTL_GET_MANAGER_UID, &cmd);
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
