#include "small_rt.h"
#include "shitty_library.h"

#include <sys/mman.h>
#include <stddef.h>

__attribute__((used))
void prep_main(long *sp)
{
	constexpr char mmap_min_addr[] = "/proc/sys/vm/mmap_min_addr";
	long fd = __syscall(SYS_openat, AT_FDCWD, (long)mmap_min_addr, O_WRONLY);
	 __syscall(SYS_write, fd, (long)(mmap_min_addr + sizeof(mmap_min_addr) - 1), 1, NONE, NONE, NONE);
	 __syscall(SYS_close, fd);

	__syscall(SYS_mmap, 0, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	
	*(volatile int *)NULL = 3;
	
	__builtin_printf("%d \n", *(volatile int *)NULL);

	__syscall(SYS_exit, 0);
	__builtin_unreachable();
}
