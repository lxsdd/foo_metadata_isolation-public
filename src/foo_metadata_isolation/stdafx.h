#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef STRICT
#define STRICT
#endif
#define _WIN32_WINNT 0x0601

// Do not define WIN32_LEAN_AND_MEAN: foobar2000 SDK headers require COM/OLE declarations.
#include <SDK/foobar2000.h>
#include <SDK/coreDarkMode.h>
#include <helpers/foobar2000+atl.h>
#include <helpers/atl-misc.h>

