/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

/* Apart from main.c: windows.h and raylib.h define the same names. */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>

#include "console.h"

void
console_attach(void)
{
#ifdef _WIN32
	/* Output redirected to a file or a pipe is already in place. */
	if (!AttachConsole(ATTACH_PARENT_PROCESS))
		return;
	if (GetStdHandle(STD_OUTPUT_HANDLE) == NULL)
		freopen("CONOUT$", "w", stdout);
	if (GetStdHandle(STD_ERROR_HANDLE) == NULL)
		freopen("CONOUT$", "w", stderr);
#endif
}
