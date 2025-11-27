#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>

// get uid from kernelsu
struct ksu_get_manager_uid_cmd {
	uint32_t uid;
};
#define KSU_IOCTL_GET_MANAGER_UID _IOC(_IOC_READ, 'K', 10, 0)
#define KSU_INSTALL_MAGIC1 0xDEADBEEF
#define KSU_INSTALL_MAGIC2 0xCAFEBABE

// for cmd, we can do a manual string to int
// then check if its between 10000 ~ 20000
// this way we can remove strtoul / strtol
// after all, int to char is just shit + 48
static int dumb_str_to_appuid(const char *str)
{
	int uid = 0;

	// dereference to see if user supplied 5 digits
	if ( !*(str + 4) )
		return uid;

	uid = *(str  + 4) - 48;
	uid = uid + ( *(str  + 3) - 48 ) * 10;
	uid = uid + ( *(str  + 2) - 48 ) * 100;
	uid = uid + ( *(str  + 1) - 48 ) * 1000;
	uid = uid + ( *str - 48 ) * 10000;

	if (!(uid > 10000 && uid < 20000))
		return 0;

	return uid;
}

/*
 * strnmatch, test two strings if they match up to n len
 *
 * caller is reposnible for sanity! no \0 check!
 * returns: 0 = match, 1 = not match
 *
 * Usage examples:
 * for strcmp like behavior, strnmatch(x, y, strlen(y) + 1) (+1 for \0)
 * for strstarts like behavior strnmatch(x, y, strlen(y))
 *  
 */
static int strnmatch(const char *a, const char *b, unsigned int count)
{
	// og condition was like *a && (*a == *b) && count > 0
	do {
		// if they arent equal
		if (*a != *b) 
			return 1;
		a++;
		b++;
		count --;
	} while (count > 0);

	// we reach here if they match
	return 0;
}

static int fail(void)
{
	const char *error = "fail\n";
	syscall(SYS_write, 2, error, strlen(error));
	return 1;
}

// https://github.com/backslashxx/various_stuff/blob/master/ksu_prctl_test/ksu_prctl_02_only.c
static int dumb_print_appuid(int uid)
{
	if (!(uid > 10000 && uid < 20000))
		return fail();

	char digits[6];

	if (uid > 10000) {
		int i = 4;
		uid = uid % 100000; // oob filter
		do {
			digits[i] = 48 + (uid % 10);
			uid = uid / 10;
			i--;			
		} while (i >= 0);

		digits[5] = '\n';
	} else
		return fail();

	syscall(SYS_write, 1, digits, 6);
	return 0;
}

static int show_usage(void)
{
	const char *usage = "Usage:\n./uidtool --setuid <uid>\n./uidtool --getuid\n";
	syscall(SYS_write, 2, usage, strlen(usage));
	return 0;
}

int main(int argc, char *argv[])
{
	if (!argv[1])
		goto bail_out;

	if (!strnmatch(argv[1], "--setuid", strlen("--setuid") + 1)) {
		int magic1 = 0xDEADBEEF;
		int magic2 = 10006;
		uintptr_t arg;
		
		unsigned int cmd = dumb_str_to_appuid(argv[2]);
		if (cmd == 0)
			return fail();
		
		syscall(SYS_reboot, magic1, magic2, cmd, (void *)&arg);

		if ( *(uintptr_t *)arg == arg ) {
			syscall(SYS_write, 2, "ok\n", strlen("ok\n"));
			return 0;
		}
		
		return fail();
	}

	if (!strnmatch(argv[1], "--getuid", strlen("--getuid") + 1)) {
		unsigned int fd = 0;
		syscall(SYS_reboot, KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, (void *)&fd);
		if (!fd)
			return fail();

		struct ksu_get_manager_uid_cmd cmd = {0};
		int ret = syscall(SYS_ioctl, fd, KSU_IOCTL_GET_MANAGER_UID, &cmd);
		if (ret)
			return fail();

		return dumb_print_appuid(cmd.uid);
	}

bail_out:
	return show_usage();

}
