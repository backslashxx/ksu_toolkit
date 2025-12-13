#include <stdint.h>
#include <sys/ioctl.h>
#include <ctype.h>

#include "small_rt.h"

// zig cc -target aarch64-linux -Oz -s -Wl,--gc-sections,--strip-all,-z,norelro -fno-unwind-tables -Wl,--entry=__start toolkit.c -o toolkit 

#define alloca __builtin_alloca
#define memcmp __builtin_memcmp

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

// sulog
struct sulog_entry {
	uint8_t symbol;
	uint32_t uid; // mebbe u16?
} __attribute__((packed));

struct sulog_entry_rcv_ptr {
	uint64_t int_ptr; // send index here
	uint64_t buf_ptr; // send buf here
};

#define SULOG_ENTRY_MAX 100
#define SULOG_BUFSIZ SULOG_ENTRY_MAX * (sizeof (struct sulog_entry))
#define GET_SULOG_DUMP 10009     // get sulog dump, max, last 100 escalations

__attribute__((noinline))
static unsigned long strlen(const char *str)
{
	const char *s = str;
	while (*s)
		s++;

	return s - str;
}

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

__attribute__((noinline))
static int dumb_print_appuid(int uid, long len)
{
	// +1 for \n
	char digits[len + 1];

	int i = len - 1;
	do {
		digits[i] = 48 + (uid % 10);
		uid = uid / 10;
		i--;			
	} while (!(i < 0));

	// char index starts at 0
	digits[len] = '\n';

	__syscall(SYS_write, 1, (long)digits, len +1, NONE, NONE, NONE);
	return 0;
}

__attribute__((always_inline))
static int c_main(int argc, char **argv, char **envp)
{
	const char *ok = "ok\n";
	const char *newline = "\n";
	const char usage[] =
	"Usage:\n"
	"./toolkit --setuid <uid>\n"
	"./toolkit --getuid\n"
	"./toolkit --getlist\n"
	"./toolkit --sulog\n";

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

		return dumb_print_appuid(cmd.uid, 5);
	}

	if (!memcmp(argv[1], "--getlist", strlen("--getlist") + 1) && !argv[2]) {
		unsigned long total_size = 0;

		__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, (long)&fd, NONE, NONE);
		if (!fd)
			goto fail;

		struct ksu_add_try_umount_cmd cmd = {0};
		cmd.arg = (uint64_t)&total_size;
		// cmd.flags = 0;
		cmd.mode = KSU_UMOUNT_GETSIZE;

		int ret = __syscall(SYS_ioctl, fd, KSU_IOCTL_ADD_TRY_UMOUNT, (long)&cmd, NONE, NONE, NONE);
		if (ret < 0)
			goto fail;

		if (!total_size)
			goto list_empty;

		// max stack size on linux is 8 * 1024 * 1024
		// this should never happen but better catch this.
		if (total_size > 8000000)
			__builtin_trap();

		// now we can prepare the same size of memory
		void *buffer = alloca(total_size);

		cmd.arg = (uint64_t)buffer;
		// cmd.flags = 0;
		cmd.mode = KSU_UMOUNT_GETLIST;

		ret = __syscall(SYS_ioctl, fd, KSU_IOCTL_ADD_TRY_UMOUNT, (long)&cmd, NONE, NONE, NONE);
		if (ret < 0)
			goto fail;

		// now we pointerwalk
		const char *char_buf = (const char *)buffer;
		while (*char_buf) {
			__syscall(SYS_write, 1, (long)char_buf, strlen(char_buf), NONE, NONE, NONE);
			__syscall(SYS_write, 1, (long)newline, 1, NONE, NONE, NONE);
			
			char_buf = char_buf + strlen(char_buf) + 1;
		}

		return 0;
	}

	if (!memcmp(argv[1], "--sulog", strlen("--sulog") + 1) && !argv[2]) {	
		uint64_t latest_index = 0;
		char sulog_buf[SULOG_BUFSIZ] = {0};
		char t[] = "sym: x uid: ";

		struct sulog_entry_rcv_ptr sbuf = {0};
		
		sbuf.int_ptr = (uint64_t)&latest_index;
		sbuf.buf_ptr = (uint64_t)sulog_buf;

		__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, GET_SULOG_DUMP, 0, (long)&sbuf, NONE, NONE);
		
		int start = latest_index;

		int i = 0;

sulog_loop_start:			
		// this way we make it so that latest index is highest index
		// modulus due to this overflowinbf entry_max
		int idx = (start + i) % SULOG_ENTRY_MAX;
		struct sulog_entry *entry_ptr = (struct sulog_entry *)(sulog_buf + idx * sizeof(struct sulog_entry) );
			
		if (entry_ptr->symbol) {
			t[5] = entry_ptr->symbol;
			__syscall(SYS_write, 1, (long)t, strlen(t), NONE, NONE, NONE);
			dumb_print_appuid(entry_ptr->uid, 6); // android appuid to 6 digits
		}

		i++;

		if (i < SULOG_ENTRY_MAX)
			goto sulog_loop_start;

		return 0;
	}

show_usage:
	return fail(usage);

list_empty:
	return fail("list empty\n");

fail:
	return fail("fail\n");
}

__attribute__((used))
void prep_main(long *sp)
{
	long argc = *sp;
	char **argv = (char **)(sp + 1);
	char **envp = argv + argc + 1; // we need to offset it by the number of argc's!

	long exit_code = c_main(argc, argv, envp);
	__syscall(SYS_exit, exit_code, NONE, NONE, NONE, NONE, NONE);
	__builtin_unreachable();
}
