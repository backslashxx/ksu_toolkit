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
	uint32_t data : 24;  // uid, uint24_t
	uint32_t sym : 8;        // symbol
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

__attribute__((always_inline))
static void __fprintf(long fd, const char *buf, unsigned long len)
{
	__syscall(SYS_write, fd, (long)buf, len, NONE, NONE, NONE);
}

__attribute__((noinline))
static void print_out(const char *buf, unsigned long len)
{
	__fprintf(1, buf, len);
}

__attribute__((noinline))
static void print_err(const char *buf, unsigned long len)
{
	__fprintf(2, buf, len);
}

/*
 * ksu_sys_reboot, small shim for ksu backdoored sys_reboot 
 *
 * magic1 is always 0xDEADBEEF (KSU_INSTALL_MAGIC1)
 * controllable magic2, cmd, arg
 *
 */
__attribute__((noinline))
static void ksu_sys_reboot(long magic2, long cmd, long arg)
{
	__syscall(SYS_reboot, KSU_INSTALL_MAGIC1, magic2, cmd, arg, NONE, NONE);
}

/*
 * sys_ioctl, literally ioctl()
 * duh
 *
 */
__attribute__((always_inline))
static int sys_ioctl(unsigned long fd, unsigned long cmd, unsigned long arg)
{
	return (int)__syscall(SYS_ioctl, fd, cmd, arg, NONE, NONE, NONE);
}

__attribute__((always_inline))
static int dumb_str_to_appuid(const char *str)
{
	int uid = 0;
	int i = 4;
	int m = 1;

start:
	// llvm actually has an optimized isdigit
	// just not prefixed with __builtin
	// code generated is the same size, so better use it
	if (!isdigit(str[i]))
		return 0;

	uid = uid + ( str[i] - '0' ) * m;
	m = m * 10;
	i--;
	
	if (!(i < 0))
		goto start;

	if (!(uid > 10000 && uid < 20000))
		return 0;

	return uid;
}

/*
 *	long_to_str, long to string
 *	
 *	converts an int to string with expected len
 *	
 *	caller is reposnible for sanity!
 *	no bounds check, no nothing, do not pass len = 0
 *	
 *	example:
 *	long_to_str(10123, 5, buf); // where buf is char buf[5]; atleast
 */

__attribute__((noinline))
static void long_to_str(unsigned long number, unsigned long len, char *buf)
{

start:
	buf[len - 1] = 48 + (number % 10);
	number = number / 10;
	len--;

	if (len > 0)
		goto start;

	return;
}

__attribute__((always_inline))
static inline int sulogv1()
{
	unsigned long sulog_index_next;
	char sulogv1_buf[SULOGV1_BUFSIZ];
	char t[] = "sym: ? uid: ??????\n";

	struct sulogv1_entry_rcv_ptr sbuf = {0};
	sbuf.int_ptr = (uint64_t)&sulog_index_next;
	sbuf.buf_ptr = (uint64_t)sulogv1_buf;

	ksu_sys_reboot(GET_SULOG_DUMP, 0, (long)&sbuf);
	
	// sulog_index_next is the oldest entry!
	// and sulog_index_next -1 is the newest entry
	// we start listing from the oldest entry
	int start = sulog_index_next;

	int i = 0;

sulogv1_loop_start:
	int idx = (start + i) % SULOGV1_ENTRY_MAX; // modulus due to this overflowing entry_max
	struct sulogv1_entry *entry_ptr = (struct sulogv1_entry *)(sulogv1_buf + idx * sizeof(struct sulogv1_entry) );

	if (entry_ptr->symbol) {
		t[5] = entry_ptr->symbol;
		long_to_str(entry_ptr->uid, 6, &t[12]);
		print_out(t, sizeof(t) - 1 );
	}

	i++;

	if (i < SULOGV1_ENTRY_MAX)
		goto sulogv1_loop_start;

	return 0;
}

__attribute__((always_inline))
static int c_main(int argc, char **argv, char **envp)
{
	const char ok[] = { 'o', 'k', '\n'};
	const char usage[] =
	"Usage:\n"
	"./toolkit --setuid <uid>\n"
	"./toolkit --getuid\n"
	"./toolkit --getlist\n"
	"./toolkit --sulog\n";

	unsigned int fd = 0;
	char *argv1 = argv[1];
	char *argv2 = argv[2];

	if (!argv1)
		goto show_usage;

	// --setuid
	if (!memcmp(&argv1[1], "-setuid", sizeof("-setuid")) && 
		!!argv2 && !!argv2[4] && !argv2[5] && !argv[3]) {
		
		unsigned int cmd = dumb_str_to_appuid(argv2);
		if (!cmd)
			goto fail;

		// yeah we reuse argv1 as buffer		
		ksu_sys_reboot(CHANGE_MANAGER_UID, cmd, (long)argv1);

		// all we need is just somethign writable that is atleast uintptr_t wide
		// since argv1 here will fit 9 bytes, a full u64 ptr can fit no issues
		if (*(uintptr_t *)argv1 != (uintptr_t)argv1 )
			goto fail;
		
		print_out(ok, sizeof(ok));
		return 0;

	}

	// --getuid
	if (!memcmp(&argv1[1], "-getuid", sizeof("-getuid")) && !argv2) {
		
		// we dont care about closing the fd, it gets released on exit automatically
		ksu_sys_reboot(KSU_INSTALL_MAGIC2, 0, (long)&fd);
		if (!fd)
			goto fail;

		struct ksu_get_manager_uid_cmd cmd;
		int ret = sys_ioctl(fd, KSU_IOCTL_GET_MANAGER_UID, (long)&cmd);
		if (ret)
			goto fail;

		if (!(cmd.uid > 10000 && cmd.uid < 20000))
			goto fail;

		// yeah we reuse argv1 as our buffer
		// this one is really just for a buffer/scratchpad
		long_to_str(cmd.uid, 5, argv1);
		argv1[5] = '\n';

		print_out(argv1, 6);
		
		return 0;
		
	}

	// --getlist
	if (!memcmp(&argv1[2], "getlist", sizeof("getlist")) && !argv2) {
		unsigned long total_size;

		ksu_sys_reboot(KSU_INSTALL_MAGIC2, 0, (long)&fd);
		if (!fd)
			goto fail;

		struct ksu_add_try_umount_cmd cmd = {0};
		cmd.arg = (uint64_t)&total_size;
		// cmd.flags = 0;
		cmd.mode = KSU_UMOUNT_GETSIZE;

		int ret = sys_ioctl(fd, KSU_IOCTL_ADD_TRY_UMOUNT, (long)&cmd);
		if (ret < 0)
			goto fail;

		if (!total_size)
			goto list_empty;

		// this costs 20 bytes, dont bother.
		//if (total_size > 8 * 1000 * 1000)
		//	__builtin_trap();

		// now we can prepare the same size of memory
		char *buffer = alloca(total_size);

		cmd.arg = (uint64_t)buffer;
		// cmd.flags = 0;
		cmd.mode = KSU_UMOUNT_GETLIST;

		ret = sys_ioctl(fd, KSU_IOCTL_ADD_TRY_UMOUNT, (long)&cmd);
		if (ret < 0)
			goto fail;

		// now we pointerwalk
		char *char_buf = buffer;

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
		uint32_t sulog_index_next;
		uint32_t sulog_uptime = 0;
		char sulog_buf[SULOG_BUFSIZ];
		char uptime_text[] = "uptime: ??????????\n";
		char text_v2[] = "sym: ? uid: ?????? time: ??????????\n";

		struct sulog_entry_rcv_ptr sbuf = {0};
		sbuf.index_ptr = (uint64_t)&sulog_index_next;
		sbuf.buf_ptr = (uint64_t)sulog_buf;
		sbuf.uptime_ptr = (uint64_t)&sulog_uptime;

		ksu_sys_reboot(GET_SULOG_DUMP_V2, 0, (long)&sbuf);

		// sulog_index_next is the oldest entry!
		// and sulog_index_next -1 is the newest entry
		// we start listing from the oldest entry
		int start = sulog_index_next;

		int i = 0;

		if (!(*(uintptr_t *)&sbuf == (uintptr_t)&sbuf) )
			return sulogv1(); // attempt v1

		long_to_str(sulog_uptime, 10, &uptime_text[8]);
		print_out(uptime_text, sizeof(uptime_text));

	sulog_loop_start:		
		int idx = (start + i) % SULOG_ENTRY_MAX; // modulus due to this overflowing entry_max
		struct sulog_entry *entry_ptr = (struct sulog_entry *)(sulog_buf + idx * sizeof(struct sulog_entry) );

		if (entry_ptr->data) {
			// now write symbol
			text_v2[5] = entry_ptr->sym;
			long_to_str(entry_ptr->data, 6, &text_v2[12]);
			long_to_str(entry_ptr->s_time, 10, &text_v2[25]);

			print_out(text_v2, sizeof(text_v2) - 1 );
		}

		i++;

		if (i < SULOG_ENTRY_MAX)
			goto sulog_loop_start;

		return 0;
	}
show_usage:
	print_err(usage, sizeof(usage) -1 );
	return 1;

list_empty:
	print_err("list empty\n", sizeof("list empty\n") - 1);
	return 1;

fail:
	print_err("fail\n", sizeof("fail\n") - 1);
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
