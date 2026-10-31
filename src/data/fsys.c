/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>		/* mkdtemp() on macOS */
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "fsys.h"

#ifdef _WIN32

int
fsys_mkdir(const char *path)
{
	return (_mkdir(path));
}

int
fsys_replace(const char *from, const char *to)
{
	if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING |
	    MOVEFILE_WRITE_THROUGH))
		return (0);
	errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : ENOENT;
	return (-1);
}

char *
fsys_mkdtemp(char *template)
{
	static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	static unsigned long counter;
	size_t len = strlen(template);
	unsigned long v;
	int i, k;

	if (len < 6 || strcmp(template + len - 6, "XXXXXX") != 0) {
		errno = EINVAL;
		return (NULL);
	}
	for (i = 0; i < 1000; i++) {
		v = GetCurrentProcessId() * 7919UL + GetTickCount() + counter++;
		for (k = 1; k <= 6; k++, v /= 36)
			template[len - k] = digits[v % 36];
		if (_mkdir(template) == 0)
			return (template);
		if (errno != EEXIST)
			break;
	}
	return (NULL);
}

#else

int
fsys_mkdir(const char *path)
{
	return (mkdir(path, 0755));
}

int
fsys_replace(const char *from, const char *to)
{
	return (rename(from, to));
}

char *
fsys_mkdtemp(char *template)
{
	return (mkdtemp(template));
}

#endif
