/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

/*
 * The directory of the running program, each system its own way; kept
 * apart from fsys.c, whose POSIX feature macro would hide these calls.
 */

#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L	/* readlink() under -std=c99 */
#endif

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdint.h>
#elif defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "fsys.h"

/* The program's own path in "buf", or 0 if the system cannot say. */
static int
self_path(char *buf, size_t len)
{
#if defined(_WIN32)
	DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)len);

	return (n > 0 && n < len);
#elif defined(__APPLE__)
	uint32_t n = (uint32_t)len;

	return (_NSGetExecutablePath(buf, &n) == 0);
#elif defined(__FreeBSD__)
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	size_t n = len;

	return (sysctl(mib, 4, buf, &n, NULL, 0) == 0 && n > 0);
#elif defined(__linux__)
	ssize_t n = readlink("/proc/self/exe", buf, len - 1);

	if (n <= 0)
		return (0);
	buf[n] = '\0';
	return (1);
#else
	(void)buf;
	(void)len;
	return (0);
#endif
}

int
fsys_program_dir(const char *argv0, char *out, size_t len)
{
	char *slash, *back;

	if (len == 0)
		return (-1);
	if (!self_path(out, len)) {
		if (argv0 == NULL || strlen(argv0) >= len)
			return (-1);
		strcpy(out, argv0);
	}
	slash = strrchr(out, '/');
	back = strrchr(out, '\\');
	if (back != NULL && (slash == NULL || back > slash))
		slash = back;
	if (slash == NULL)
		strcpy(out, ".");
	else if (slash == out)
		out[1] = '\0';
	else
		*slash = '\0';
	return (0);
}
