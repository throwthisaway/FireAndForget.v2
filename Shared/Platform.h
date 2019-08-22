#pragma once
#ifdef _WIN32
	#define PLATFORM_WIN
	#undef max
	#undef min
	#ifdef _WIN64
		#define PLATFORM_WIN64
	#else
		#define PLATFORM_WIN32
	#endif
#elif __APPLE__
	#include "TargetConditionals.h"
	#if TARGET_IPHONE_SIMULATOR
		#define PLATFORM_IOS_SIM
	#elif TARGET_OS_IPHONE
		#define PLATFORM_IOS
	#elif TARGET_OS_MAC
		#define PLATFORM_MAC_OS
	#else
	#   error "Unknown Apple platform"
	#endif
#elif __linux__
// linux
#elif __unix__ // all unices not caught above
// Unix
#elif defined(_POSIX_VERSION)
// POSIX
#elif __EMSCRIPTEN__
	#define PLATFORM_EMSCRIPTEN
#else
#   error "Unknown compiler"
#endif