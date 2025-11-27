#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdint.h>

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

static int fail(void)
{
	const char *error = "fail\n";
	syscall(SYS_write, 2, error, strlen(error));
	return 1;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		const char *error = "Usage: ./change_uid <uid>\n";
		syscall(SYS_write, 2, error, strlen(error));
		return 1;
	}
	

	int magic1 = 0xDEADBEEF;
	int magic2 = 10006;
	uintptr_t arg;
	
	unsigned int cmd = dumb_str_to_appuid(argv[1]);
	if (cmd == 0)
		return fail();
	
	syscall(SYS_reboot, magic1, magic2, cmd, (void *)&arg);

#if 0 // we drop stdlib
	printf("SYS_reboot(0x%x, %d, %u, %p)\n", magic1, magic2, cmd, (void *)&arg);
	// if our arg contains our pointer then its good
	printf("reply: 0x%lx verdict: %s\n", *(uintptr_t *)arg, *(uintptr_t *)arg == (uintptr_t)arg ? "ok" : "fail" );
#endif
	
	if ( *(uintptr_t *)arg == arg )
		syscall(SYS_write, 2, "ok\n", strlen("ok\n"));
	else
		return fail();
	
	return 0;

}
