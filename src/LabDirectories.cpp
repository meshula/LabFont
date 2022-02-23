
#include "LabDirectories.h"
#include <string.h>

#ifdef _WIN32
# include <Windows.h>
#elif defined(__APPLE__)
# include <mach-o/dyld.h>
#elif defined(__linux__)
# include <unistd.h>
#endif

// https://stackoverflow.com/questions/11238918/s-isreg-macro-undefined Windows
// does not define the S_ISREG and S_ISDIR macros in stat.h, so we do. We have
// to define _CRT_INTERNAL_NONSTDC_NAMES 1 before #including sys/stat.h in
// order for Microsoft's stat.h to define names like S_IFMT, S_IFREG, and
// S_IFDIR, rather than just defining  _S_IFMT, _S_IFREG, and _S_IFDIR as it
// normally does.
#define _CRT_INTERNAL_NONSTDC_NAMES 1
#include <sys/stat.h>
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
  #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifdef _WIN32

const char* lab_application_executable_path(const char * argv0)
{
    static char buf[1024] = { 0 };
    if (!buf[0]) {
        DWORD ret = GetModuleFileNameA(NULL, buf, sizeof(buf));
        if (ret == 0)
            strncpy(buf, argv0, sizeof(buf));
    }

    return buf;
}

#elif defined(__APPLE__)

const char* lab_application_executable_path(const char * argv0)
{
    static char buf[1024] = { 0 };
    if (!buf[0]) {
        uint32_t size = sizeof(buf);
        int ret = _NSGetExecutablePath(buf, &size);
        if (0 == ret)
            strncpy(buf, argv0, sizeof(buf));
    }
    return buf;
}

#elif defined(__linux__)

const char* lab_application_executable_path(const char * argv0)
{
    static char buf[1024] = { 0 };
    if (!buf[0]) {
        ssize_t size = readlink("/proc/self/exe", buf, sizeof(buf));
        if (size == 0)
            strncpy(buf, argv0, sizeof(buf));
    }
    return buf;
}

#else
# error Unknown platform
#endif

static void remove_filename(char* fn)
{
    if (!fn)
        return;

    char* sep = strrchr(fn, '\\');
    if (sep) {
        ++sep;
        *sep = '\0';
        return;
    }
    sep = strrchr(fn, '/');
    if (sep) {
        ++sep;
        *sep = '\0';
    }
}

static void remove_sep(char* buf)
{
    if (!buf)
        return;
    int sz = strlen(buf);
    if (sz > 0 && (buf[sz - 1] == '/' || buf[sz - 1] == '\\'))
        buf[sz - 1] = '\0';
}

static void add_filename(char* buf, const char* fn) {
    if (!buf || !fn)
        return;
    int sz = strlen(buf);
    if (sz > 0 && (buf[sz - 1] != '/' && buf[sz - 1] != '\\')) {
        buf[sz] = '/';
        buf[sz+1] = '\0';
    }
    strcat(buf, fn);
}

static void normalize_path(char* buf) {
    if (!buf)
        return;
    int sz = strlen(buf);
    for (int i = 0; i < sz; ++i)
        if (buf[i] == '\\')
            buf[i] = '/';
}

const char* lab_application_resource_path(const char * argv0, const char* rsrc)
{
    if (!argv0 || !rsrc)
        return nullptr;

    static bool test = true;
    static char buf[1024] = { 0 };
    if (test) {
        test = false;
        char check[1024];
        strncpy(check, argv0, sizeof(check));
        check[1023] = '\0';

        int sz = 0;
        do {
            remove_sep(check);
            sz = strlen(check);
            remove_filename(check);
            int new_sz = strlen(check);
            if (new_sz == sz)
                return nullptr;
            sz = new_sz;

            strncpy(buf, check, sizeof(buf));
            add_filename(buf, rsrc);
            normalize_path(buf);
            struct stat sb;
            int res = stat(buf, &sb);
            if (res == 0 && S_ISDIR(sb.st_mode)) {
                return buf;
            }

            strncpy(buf, check, sizeof(buf));
            add_filename(buf, "install");
            add_filename(buf, rsrc);
            normalize_path(buf);
            res = stat(buf, &sb);
            if (res == 0 && S_ISDIR(sb.st_mode)) {
                return buf;
            }

        } while (true);
    }
    buf[0] = '\0';
    return buf;
}
