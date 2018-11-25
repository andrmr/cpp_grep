#include "util/sys.h"

#ifdef UNIX_BUILD
#    include <unistd.h>
#elif defined WIN32_BUILD
#    include <windows.h>
#endif

namespace util::sys {

#if !(defined UNIX_BUILD || defined WIN32_BUILD)
namespace {
constexpr auto DEFAULT_PAGESIZE {4096UL}; //!< Default pagesize, in bytes.
}
#endif

unsigned long pagesize() noexcept
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

#ifdef WIN32_BUILD
#    include <securitybaseapi.h>
bool win32_can_read(const char* path) noexcept
{
    // NOTE: based on implementation from http://blog.aaronballman.com/2011/08/how-to-check-access-rights/
    // TODO: use generic C++ and STL where possible; replace malloc with unique_ptr

    auto bRet {false};
    DWORD genericAccessRights = GENERIC_READ;

    DWORD length = 0;
    if (!GetFileSecurity(path, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, nullptr, 0, &length) && ERROR_INSUFFICIENT_BUFFER == ::GetLastError())
    {
        auto security = static_cast<PSECURITY_DESCRIPTOR>(::malloc(length));
        if (security && GetFileSecurity(path, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, security, length, &length))
        {
            HANDLE hToken = nullptr;
            if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ, &hToken))
            {
                HANDLE hImpersonatedToken = nullptr;
                if (::DuplicateToken(hToken, SecurityImpersonation, &hImpersonatedToken))
                {
                    GENERIC_MAPPING mapping  = {0xFFFFFFFF};
                    PRIVILEGE_SET privileges = {0};
                    DWORD grantedAccess = 0, privilegesLength = sizeof(privileges);
                    BOOL result = FALSE;

                    mapping.GenericRead = FILE_GENERIC_READ;

                    ::MapGenericMask(&genericAccessRights, &mapping);
                    if (::AccessCheck(security, hImpersonatedToken, genericAccessRights,
                                      &mapping, &privileges, &privilegesLength, &grantedAccess, &result))
                    {
                        bRet = (result == TRUE);
                    }
                    ::CloseHandle(hImpersonatedToken);
                }
                ::CloseHandle(hToken);
            }
        }
        free(security);
    }

    return bRet;
}
#endif

} // namespace util::sys
