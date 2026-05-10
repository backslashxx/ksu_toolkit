__attribute__((always_inline))
static int toolkit_ksu_override_common(long cmdtype, char *restrict argv2, void *restrict sp){

	int override = 0;
	if (!argv2)
		goto send;

	override = dumb_atoi(argv2);
	if (!override)
		return 1;
		
send:
	ksu_sys_reboot(cmdtype, override, (long)sp);
	if ( *(uintptr_t *)sp != (uintptr_t)sp )
		return 1;
	
	return 0;
}

__attribute__((always_inline))
static int toolkit_main(long argc, char **argv, char **envp)
{
	const char ok[] = { 'o', 'k', '\n'};
	const char usage[] =
	"Usage:\n"
	"./toolkit --bench\n"
	"./toolkit --setuid <uid>\n"
	"./toolkit --getuid\n"
	"./toolkit --getlist\n"
	"./toolkit --getinfo\n"
	"./toolkit --sulog\n"
	"./toolkit --setver <? uint>\n"
	"./toolkit --setflags <? uint>\n"
	"./toolkit --fkuname \"6.18\" \"#0 SMP ...\"\n"
	;

	unsigned int fd = 0;

	// use once needed, for -Oz atleast, these are put to registers anyway
	// register char *argv1 __asm__("x19") = argv[1];
	// register char *argv2 __asm__("x20") = argv[2];

	char *argv1 = argv[1];
	char *argv2 = argv[2];
	char *sp = (char *)argv - sizeof(long);

	if (!argv1)
		goto show_usage;

	// --bench
	if (!memcmp(&argv1[0], "--bench", sizeof("--bench")) && !argv2) {
		bench_main();
		return 0;

	}

	// --setuid
	if (!memcmp(&argv1[1], "-setuid", sizeof("-setuid")) && !!argv2 && !!argv2[4] && !argv[3]) {
		
		unsigned int cmd = dumb_atoi(argv2) % 100000;
		if (!cmd)
			goto fail;

		if (!(cmd > 10000 && cmd < 20000))
			goto fail;

		// yeah we reuse argv1 as buffer		
		ksu_sys_reboot(CHANGE_MANAGER_UID, cmd, (long)sp);

		// all we need is just somethign writable that is atleast uintptr_t wide
		// since *sp is long as that is our argc, this will fit platform's uintptr_t
		// while being properly aligned and passing ubsan
		if (*(uintptr_t *)sp != (uintptr_t)sp )
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

		struct ksu_get_manager_uid_cmd *cmd = (struct ksu_get_manager_uid_cmd *)sp;
		int ret = sys_ioctl(fd, KSU_IOCTL_GET_MANAGER_UID, (long)cmd);
		if (ret)
			goto fail;

		if (!(cmd->uid > 10000 && cmd->uid < 20000))
			goto fail;

		// yeah we reuse argv1 as our buffer
		// this one is really just for a buffer/scratchpad
		dumb_itoa(cmd->uid, 5, argv1);
		argv1[5] = '\n';

		print_out(argv1, 6);
		return 0;
		
	}

	// --getlist
	if (!memcmp(&argv1[2], "getlist", sizeof("getlist")) && !argv2) {
		uint32_t total_size;

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

		// now we can prepare some memory
		char *buffer = toolkit_malloc(total_size );
		if (!buffer)
			goto fail;

		cmd.arg = (uint64_t)buffer;
		// cmd.flags = 0;
		cmd.mode = KSU_UMOUNT_GETLIST;

		ret = sys_ioctl(fd, KSU_IOCTL_ADD_TRY_UMOUNT, (long)&cmd);
		if (ret < 0)
			goto fail;

		// now we pointerwalk
		char *char_buf = buffer;
		int len;

	bufwalk_start:
		// get entry's string length first
		len = strlen(char_buf);

		// write a newline to it, basically replacing \0 with \n
		*(char_buf + len) = '\n';

		// walk the pointer
		char_buf = char_buf + len + 1;

		if (char_buf - buffer < total_size)
			goto bufwalk_start;

		print_out(buffer, total_size);

		return 0;
	}

	if (!memcmp(argv1, "--sulog", sizeof("--sulog")) && !argv2) {
		uint32_t sulog_index_next;
		uint32_t sulog_uptime = 0;
		char uptime_text[] = "uptime: ??????????\n";
		char text_v2[] = "sym: ? uid: ?????? time: ??????????\n";
		char *sulog_buf = sp;

		struct sulog_entry_rcv_ptr sbuf = {0};
		sbuf.index_ptr = (uint64_t)&sulog_index_next;
		sbuf.buf_ptr = (uint64_t)sulog_buf;
		sbuf.uptime_ptr = (uint64_t)&sulog_uptime;

		ksu_sys_reboot(GET_SULOG_DUMP_V2, 0, (long)&sbuf);

		if (*(uintptr_t *)&sbuf != (uintptr_t)&sbuf)
			goto fail;

		// sulog_index_next is the oldest entry!
		// and sulog_index_next -1 is the newest entry
		// we start listing from the oldest entry
		int start = sulog_index_next;

		int i = 0;
		int idx;

		dumb_itoa(sulog_uptime, 10, &uptime_text[8]);
		print_out(uptime_text, sizeof(uptime_text));

	sulog_loop_start:		
		idx = (start + i) % SULOG_ENTRY_MAX; // modulus due to this overflowing entry_max
		struct sulog_entry *entry_ptr = (struct sulog_entry *)(sulog_buf + idx * sizeof(struct sulog_entry) );

		// make sure to check for symbol instead!
		if (entry_ptr->sym) {
			// now write symbol
			text_v2[5] = entry_ptr->sym;
			dumb_itoa(entry_ptr->uid, 6, &text_v2[12]);
			dumb_itoa(entry_ptr->s_time, 10, &text_v2[25]);

			print_out(text_v2, sizeof(text_v2) - 1 );
		}

		i++;

		if (i < SULOG_ENTRY_MAX)
			goto sulog_loop_start;

		return 0;
	}

	// --setver
	if (!memcmp(&argv1[1], "-setver", sizeof("-setver"))) {
		int ret = toolkit_ksu_override_common(CHANGE_KSUVER, argv2, sp);
		if (ret)
			goto fail;

		print_out(ok, sizeof(ok));
		return 0;

	}

	// --setflags
	if (!memcmp(&argv1[3], "etflags", sizeof("etflags"))) {
		int ret = toolkit_ksu_override_common(CHANGE_KSUFLAGS, argv2, sp);
		if (ret)
			goto fail;

		print_out(ok, sizeof(ok));
		return 0;

	}

	// --getinfo
	if (!memcmp(&argv1[2], "getinfo", sizeof("getinfo"))) {
		char buf_version[] = "ksuver: ??????\n";
		char buf_flags[] = "flags: ??????\n";
		char buf_features[] = "features: ??????\n";

		ksu_sys_reboot(KSU_INSTALL_MAGIC2, 0, (long)&fd);
		if (!fd)
			goto fail;

		struct ksu_get_info_cmd *cmd = (struct ksu_get_info_cmd *)sp;
		int ret = sys_ioctl(fd, KSU_IOCTL_GET_INFO, (long)cmd);
		if (ret)
			goto fail;

		dumb_itoa(cmd->version, 6, &buf_version[8]);
		print_out(buf_version, sizeof(buf_version) - 1);

		dumb_itoa(cmd->flags, 6, &buf_flags[7]);
		print_out(buf_flags, sizeof(buf_flags) - 1);

		dumb_itoa(cmd->features, 6, &buf_features[10]);
		print_out(buf_features, sizeof(buf_features) - 1);

		return 0;

	}

	// --spoof-uname
	if (!memcmp(&argv1[2], "fkuname", sizeof("fkuname")) && argv2 && argv[3] && !argv[4]) {

		// here we pack argv2's address 
		// basically so we can send it by reference
		// while forcing a 64 bit width
		*(uint64_t *)sp = (uint64_t)&argv2;

		ksu_sys_reboot(CHANGE_SPOOF_UNAME, 0, (long)sp);

		if ( *(uintptr_t *)sp != (uintptr_t)sp )
			goto fail;

		print_out(ok, sizeof(ok));
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

