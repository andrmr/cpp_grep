#include "util/sys.h"

#if defined(unix) || defined(__unix__) || defined(__unix)
#    define UNIX_BUILD
#    include <unistd.h>
#elif (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
#    define WIN32_BUILD
#    include <windows.h>
#else
namespace {
constexpr auto DEFAULT_PAGESIZE {4096L};
}
#endif

long util::sys::pagesize() noexcept
{
#ifdef UNIX_BUILD
    return sysconf(_SC_PAGESIZE);
#elif defined WIN32_BUILD
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
#else
    return DEFAULT_PAGESIZE;
#endif
}
