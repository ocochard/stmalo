/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

#ifndef FLIBUSTIER_FSYS_H
#define FLIBUSTIER_FSYS_H

#include <stddef.h>

/*
 * The few file system calls that POSIX and Windows spell differently.
 * Each returns what its POSIX namesake returns and sets errno.
 */

/* mkdir(path, 0755). */
int	fsys_mkdir(const char *path);

/* rename(), replacing an existing destination on Windows too. */
int	fsys_replace(const char *from, const char *to);

/* mkdtemp(): template ends in XXXXXX, replaced by the new directory's name. */
char	*fsys_mkdtemp(char *template);

/*
 * The directory the running program is in, from the system, else from
 * "argv0" (may be NULL): 0, or -1 when neither says or "len" is too short.
 */
int	fsys_program_dir(const char *argv0, char *out, size_t len);

/*
 * The user's language as a lowercase ISO 639 code ("fr"), from LC_ALL,
 * LC_MESSAGES or LANG, else the system's setting: 0, or -1 when unknown.
 */
int	fsys_user_lang(char *out, size_t len);

#endif
