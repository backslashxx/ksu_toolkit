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
#define KSU_IOCTL_ADD_TRY_UMOUNT _IOC(_IOC_WRITE, 'K', 18, 0)

#define KSU_INSTALL_MAGIC1 0xDEADBEEF
#define KSU_INSTALL_MAGIC2 0xCAFEBABE

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

// magic numbers for custom interfaces
#define CHANGE_MANAGER_UID 10006
#define KSU_UMOUNT_GETSIZE 107   // get list size // shit is u8 we cant fit 10k+ on it
#define KSU_UMOUNT_GETLIST 108   // get list
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
static int fail(const char *error, unsigned long len)
{
	__syscall(SYS_write, 2, (long)error, len, NONE, NONE, NONE);
	return 1;
}

__attribute__((noinline))
static int dumb_print_appuid(int uid, unsigned long len)
{
	// +1 for \n
	char digits[len + 1];

	int i = len - 1;
	while (!(i < 0)) {
		digits[i] = 48 + (uid % 10);
		uid = uid / 10;
		i--;			
	} 

	// char index starts at 0
	digits[len] = '\n';

	__syscall(SYS_write, 1, (long)digits, len +1, NONE, NONE, NONE);
	return 0;
}

const char *text_blob =
	"ok\n"
	"fail\n"
	"list empty\n"
	"Usage:\n"
	"./toolkit --setuid <uid>\n"
	"./toolkit --getuid\n"
	"./toolkit --getlist\n"
	"./toolkit --sulog\n";

__attribute__((always_inline))
static int c_main(int argc, char **argv, char **envp)
{
	unsigned int fd = 0;

	if (!argv[1])
		goto show_usage;

	if (!memcmp(argv[1], "--setuid", strlen("--setuid") + 1) && 
		!!argv[2] && !!argv[2][4] && !argv[2][5] && !argv[3]) {
		int magic1 = KSU_INSTALL_MAGIC1;
		int magic2 = CHANGE_MANAGER_UID;
		uintptr_t arg = 0;
		
		unsigned int cmd = dumb_str_to_appuid(argv[2]);
		if (!cmd)
			goto fail;
		
		__syscall(SYS_reboot, magic1, magic2, cmd, (long)&arg, NONE, NONE);

		if (arg && *(uintptr_t *)arg == arg ) {
			__syscall(SYS_write, 2, (long)text_blob, 3, NONE, NONE, NONE);
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
			__syscall(SYS_write, 1, (long)(text_blob + 2), 1, NONE, NONE, NONE);
			
			char_buf = char_buf + strlen(char_buf) + 1;
		}

		return 0;
	}

	if (!memcmp(argv[1], "--sulog", strlen("--sulog") + 1) && !argv[2]) {	
		unsigned long sulog_index_next = 0;
		char sulog_buf[SULOG_BUFSIZ] = {0};
		char t[] = "sym: ? uid: ";

		struct sulog_entry_rcv_ptr sbuf = {0};
		
		sbuf.int_ptr = (uint64_t)&sulog_index_next;
		sbuf.buf_ptr = (uint64_t)sulog_buf;

		__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, GET_SULOG_DUMP, 0, (long)&sbuf, NONE, NONE);
		
		// sulog_index_next is the oldest entry!
		// and sulog_index_next -1 is the newest entry
		// we start listing from the oldest entry
		int start = sulog_index_next;

		int i = 0;

	sulog_loop_start:			
		int idx = (start + i) % SULOG_ENTRY_MAX; // modulus due to this overflowing entry_max
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
	return fail(text_blob + 19, __builtin_strlen(text_blob + 19) );

list_empty:
	return fail(text_blob + 8, __builtin_strlen(text_blob + 8) );

fail:
	return fail(text_blob + 3, __builtin_strlen(text_blob + 3) );
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
