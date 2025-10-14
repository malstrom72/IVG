/**
	This intentionally minimal config is used for MSVC builds.
	The autoconf-generated header defined POSIX-only macros like `HAVE_UNISTD_H`,
	which pulls in `<unistd.h>` and breaks compilation on Windows. Leaving the
	feature macros undefined lets libpng fall back to its built-in defaults.
**/
#pragma once

#define PNG_ARM_NEON_OPT 0
#define PNG_ARM_NEON_IMPLEMENTATION 0
