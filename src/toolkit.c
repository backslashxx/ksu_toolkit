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

// sulog v1
struct sulogv1_entry {
	uint8_t symbol;
	uint32_t uid; // mebbe u16?
} __attribute__((packed));

struct sulogv1_entry_rcv_ptr {
	uint64_t int_ptr; // send index here
	uint64_t buf_ptr; // send buf here
};

#define SULOGV1_ENTRY_MAX 100
#define SULOGV1_BUFSIZ SULOGV1_ENTRY_MAX * (sizeof (struct sulogv1_entry))

// sulog v2, timestamped version, 250 entries, 8 bytes per entry
struct sulog_entry {
	uint32_t s_time; // uptime in seconds
	uint32_t data; // uint8_t[0,1,2] = uid, basically uint24_t, uint8_t[3] = symbol
} __attribute__((packed));

struct sulog_entry_rcv_ptr {
	uint64_t index_ptr; // send index here
	uint64_t buf_ptr; // send buf here
	uint64_t uptime_ptr; // uptime
};

#define SULOG_ENTRY_MAX 250
#define SULOG_BUFSIZ SULOG_ENTRY_MAX * (sizeof (struct sulog_entry))

// magic numbers for custom interfaces
#define CHANGE_MANAGER_UID 10006
#define KSU_UMOUNT_GETSIZE 107   // get list size // shit is u8 we cant fit 10k+ on it
#define KSU_UMOUNT_GETLIST 108   // get list
#define GET_SULOG_DUMP 10009     // get sulog dump, max, last 100 escalations
#define GET_SULOG_DUMP_V2 10010  // get sulog dump, timestamped, last 250 escalations

__attribute__((noinline))
static unsigned long strlen(const char *str)
{
	const char *s = str;
	while (*s)
		s++;

	return s - str;
}

__attribute__((noinline))
static void __fprintf(long fd, const char *buf, unsigned long len)
{
	__syscall(SYS_write, fd, (long)buf, len, NONE, NONE, NONE);
}

__attribute__((always_inline))
static void print_out(const char *buf, unsigned long len)
{
	__fprintf(1, buf, len);
}

__attribute__((always_inline))
static void print_err(const char *buf, unsigned long len)
{
	__fprintf(2, buf, len);
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

/*
 *	long_to_str_wn, long to string
 *	
 *	converts an int to string with expected len
 *	
 *	caller is reposnible for sanity!
 *	no bounds check, no nothing
 *	
 *	example:
 *	long_to_str_wn(10123, 5, buf); // where buf is char buf[5]; atleast
 */

__attribute__((noinline))
static void long_to_str(long number, unsigned long len, char *buf)
{
	int i = len - 1;
	while (!(i < 0)) {
		buf[i] = 48 + (number % 10);
		number = number / 10;
		i--;			
	}
	return;
}

__attribute__((always_inline))
static int c_main(int argc, char **argv, char **envp)
{
	const char *ok = "ok\n";
	const char *usage =
	"Usage:\n"
	"./toolkit --setuid <uid>\n"
	"./toolkit --getuid\n"
	"./toolkit --getlist\n"
	"./toolkit --sulog\n"
	"./toolkit --sulog2\n";

	unsigned int fd = 0;
	char *argv1 = argv[1];
	char *argv2 = argv[2];

	if (!argv1)
		goto show_usage;

	if (!memcmp(argv1, "--setuid", sizeof("--setuid")) && 
		!!argv2 && !!argv2[4] && !argv2[5] && !argv[3]) {
		int magic1 = KSU_INSTALL_MAGIC1;
		int magic2 = CHANGE_MANAGER_UID;
		uintptr_t arg = 0;
		
		unsigned int cmd = dumb_str_to_appuid(argv2);
		if (!cmd)
			goto fail;
		
		__syscall(SYS_reboot, magic1, magic2, cmd, (long)&arg, NONE, NONE);

		if (arg && *(uintptr_t *)arg == arg ) {
			print_out(ok, strlen(ok));
			return 0;
		}
		
		goto fail;
	}

	if (!memcmp(argv1, "--getuid", sizeof("--getuid")) && !argv2) {
		
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

		char gbuf[6] = { [5] = '\n' };

		long_to_str(cmd.uid, 5, gbuf);

		print_out(gbuf, sizeof(gbuf));
		
		return 0;
		
	}

	if (!memcmp(argv1, "--getlist", sizeof("--getlist")) && !argv2) {
		unsigned long total_size;

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
		char *char_buf = (char *)buffer;

	bufwalk_start:
		// get entry's string length first
		int len = strlen(char_buf);

		// write a newline to it, basically replacing \0 with \n
		*(char_buf + len) = '\n';

		// walk the pointer
		char_buf = char_buf + len + 1;
		
		if (*char_buf)
			goto bufwalk_start;

		// compiler will figure that this aliases and reuse variables 
		// rather than reviving 'buffer' use 'char_buf - total_size'
		// yes this is optimizing for size
		print_out(char_buf - total_size, total_size);	

		return 0;
	}

	if (!memcmp(argv1, "--sulog", sizeof("--sulog")) && !argv2) {
		unsigned long sulog_index_next;
		char sulog_buf[SULOGV1_BUFSIZ];
		char t[] = "sym: ? uid: ??????";

		struct sulogv1_entry_rcv_ptr sbuf = {0};
		
		sbuf.int_ptr = (uint64_t)&sulog_index_next;
		sbuf.buf_ptr = (uint64_t)sulog_buf;

		__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, GET_SULOG_DUMP, 0, (long)&sbuf, NONE, NONE);
		
		// sulog_index_next is the oldest entry!
		// and sulog_index_next -1 is the newest entry
		// we start listing from the oldest entry
		int start = sulog_index_next;

		int i = 0;

	sulogv1_loop_start:			
		int idx = (start + i) % SULOGV1_ENTRY_MAX; // modulus due to this overflowing entry_max
		struct sulogv1_entry *entry_ptr = (struct sulogv1_entry *)(sulog_buf + idx * sizeof(struct sulogv1_entry) );

		// NOTE: we replace \0 with \n on the buffer
		// so we cannot use strlen on the print, as there will be no null term on the buffer
		if (entry_ptr->symbol) {
			t[5] = entry_ptr->symbol;
			long_to_str(entry_ptr->uid, 5, &t[12]);
			t[strlen(t)] = '\n';
			print_out(t, sizeof(t));
		}

		i++;

		if (i < SULOGV1_ENTRY_MAX)
			goto sulogv1_loop_start;

		return 0;
	}

	if (!memcmp(argv1, "--sulog2", sizeof("--sulog2")) && !argv2) {
		uint32_t sulog_index_next;
		uint32_t sulog_uptime = 0;
		char sulog_buf[SULOG_BUFSIZ];
		char uptime_text[] = "uptime: ???????????";

		uptime_text[strlen(uptime_text)] = '\n';

		char text_v2[] = "sym: ? uid ?????? time: ???????????";
		text_v2[strlen(text_v2)] = '\n';

		struct sulog_entry_rcv_ptr sbuf = {0};
		
		sbuf.index_ptr = (uint64_t)&sulog_index_next;
		sbuf.buf_ptr = (uint64_t)sulog_buf;
		sbuf.uptime_ptr = (uint64_t)&sulog_uptime;

		__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, GET_SULOG_DUMP_V2, 0, (long)&sbuf, NONE, NONE);
		
		int start = sulog_index_next;

		int i = 0;

		if (!(*(uintptr_t *)&sbuf == (uintptr_t)&sbuf) )
			goto fail;

		long_to_str(sulog_uptime, 11, &uptime_text[8]);
		print_out(uptime_text, sizeof(uptime_text));

	sulog_loop_start:		
		int idx = (start + i) % SULOG_ENTRY_MAX; // modulus due to this overflowing entry_max
		struct sulog_entry *entry_ptr = (struct sulog_entry *)(sulog_buf + idx * sizeof(struct sulog_entry) );

		if (entry_ptr->data) {
			// now write symbol
			text_v2[5] = *((char *)&(*entry_ptr).data + 3);

			// now write uid
			// WARNING! Little Endian only!
			uint8_t buf[4] = {0}; // uint32_t equivalent
			buf[0] = *((char *)&(*entry_ptr).data + 0);
			buf[1] = *((char *)&(*entry_ptr).data + 1);
			buf[2] = *((char *)&(*entry_ptr).data + 2);

			// force dereference as uint32_t
			long_to_str(*(uint32_t *)&buf, 6, &text_v2[11]);

			long_to_str(entry_ptr->s_time, 11, &text_v2[24]);

			print_out(text_v2, sizeof(text_v2));
		}

		i++;

		if (i < SULOG_ENTRY_MAX)
			goto sulog_loop_start;

		return 0;
	}
show_usage:
	print_err(usage, strlen(usage));
	return 1;

list_empty:
	print_err("list empty\n", strlen("list empty\n"));
	return 1;

fail:
	print_err("fail\n", strlen("fail\n"));
	return 1;
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
