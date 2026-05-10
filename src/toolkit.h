#pragma once

#include <stdint.h>

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

// sulog v2, timestamped version, 250 entries, 8 bytes per entry
struct sulog_entry {
	uint32_t s_time; // uptime in seconds
	uint32_t uid : 24;  // uid, uint24_t
	uint32_t sym : 8;        // symbol
} __attribute__((packed));

struct sulog_entry_rcv_ptr {
	uint64_t index_ptr; // send index here
	uint64_t buf_ptr; // send buf here
	uint64_t uptime_ptr; // uptime
};

struct ksu_get_info_cmd {
	uint32_t version; // Output: KERNEL_SU_VERSION
	uint32_t flags; // Output: KSU_GET_INFO_FLAG_* bits
	uint32_t features; // Output: max feature ID supported
};
#define KSU_IOCTL_GET_INFO _IOC(_IOC_READ, 'K', 2, 0)

#define SULOG_ENTRY_MAX 250
#define SULOG_BUFSIZ SULOG_ENTRY_MAX * (sizeof (struct sulog_entry))

// magic numbers for custom interfaces
#define CHANGE_MANAGER_UID 10006
#define KSU_UMOUNT_GETSIZE 107   // get list size // shit is u8 we cant fit 10k+ on it
#define KSU_UMOUNT_GETLIST 108   // get list
#define GET_SULOG_DUMP 10009     // deprecated
#define GET_SULOG_DUMP_V2 10010  // get sulog dump, timestamped, last 250 escalations
#define CHANGE_KSUVER 10011     // change ksu version
#define CHANGE_SPOOF_UNAME 10012 // spoof uname release
#define CHANGE_KSUFLAGS 10013     // change ksuflags, do the bit calc on your own, 1 + 2 + 4 + 8 blah blah

/**
 *
 * NOTE on ksu flags:
 * #define KSU_GET_INFO_FLAG_LKM (1U << 0)		1
 * #define KSU_GET_INFO_FLAG_MANAGER (1U << 1)		2  <-- always enable this
 * #define KSU_GET_INFO_FLAG_LATE_LOAD (1U << 2)	4	
 * #define KSU_GET_INFO_FLAG_PR_BUILD (1U << 3)		8
 * 
 */
 
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
