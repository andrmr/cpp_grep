#include "sysutils.h"

#if defined(unix) || defined(__unix__) || defined(__unix)
	#define UNIX_BUILD
    #include <unistd.h>
#elif (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
	#define WIN32_BUILD
    #include <windows.h>
#else
    constexpr auto DEFAULT_PAGESIZE {4096L};
#endif

/// Retrieves the operating system's pagesize value.
long pagesize() noexcept
{
#ifdef UNIX_BUILD
    return sysconf(_SC_PAGESIZE);
#elif WIN32_BUILD
    SYSTEM_INFO system_info;
    GetSystemInfo (&system_info);
    return system_info.dwPageSize;
#else
    return DEFAULT_PAGESIZE;
#endif
}
