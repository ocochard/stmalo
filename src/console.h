/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

#ifndef FLIBUSTIER_CONSOLE_H
#define FLIBUSTIER_CONSOLE_H

/*
 * On Windows the game is a windowed program, so a double click opens no
 * console; started from one, it writes its messages there all the same.
 * Nothing elsewhere.
 */
void	console_attach(void);

#endif
