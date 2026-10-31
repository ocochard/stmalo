/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

/*
 * The user's language, from the locale variables, else from the system's
 * own setting (Windows display language, macOS preferred languages).
 */

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "fsys.h"

/*
 * The language part of a locale name ("fr_FR.UTF-8", "de-DE", "en"),
 * lowercased into "out": 1, or 0 when it has none ("C", "POSIX").
 */
static int
lang_part(const char *name, char *out, size_t len)
{
	size_t n;

	for (n = 0; name[n] != '\0' && strchr("_-.@", name[n]) == NULL; n++)
		if (n + 1 >= len || !((name[n] >= 'a' && name[n] <= 'z') ||
		    (name[n] >= 'A' && name[n] <= 'Z')))
			return (0);
	if (n < 2 || n > 3)
		return (0);
	for (out[n] = '\0'; n-- > 0;)
		out[n] = (char)(name[n] | 0x20);
	return (1);
}

/* The system's own setting, when the locale variables do not say. */
static int
system_lang(char *out, size_t len)
{
#if defined(_WIN32)
	char name[16];
	LCID id = MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT);

	return (GetLocaleInfoA(id, LOCALE_SISO639LANGNAME, name,
	    sizeof(name)) > 0 && lang_part(name, out, len));
#elif defined(__APPLE__)
	CFArrayRef langs = CFLocaleCopyPreferredLanguages();
	char name[64];
	int ok = 0;

	if (langs == NULL)
		return (0);
	if (CFArrayGetCount(langs) > 0 &&
	    CFStringGetCString(CFArrayGetValueAtIndex(langs, 0), name,
	    sizeof(name), kCFStringEncodingUTF8))
		ok = lang_part(name, out, len);
	CFRelease(langs);
	return (ok);
#else
	(void)out;
	(void)len;
	return (0);
#endif
}

int
fsys_user_lang(char *out, size_t len)
{
	static const char *vars[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
	const char *v;
	size_t i;

	/* The first one set decides, as in setlocale(). */
	for (i = 0; i < sizeof(vars) / sizeof(vars[0]); i++)
		if ((v = getenv(vars[i])) != NULL && v[0] != '\0') {
			if (lang_part(v, out, len))
				return (0);
			break;
		}
	return (system_lang(out, len) ? 0 : -1);
}
