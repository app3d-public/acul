#ifdef _WIN32
#include <windows.h>
#include <acul/exception/exception.hpp>

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH) acul::destroy_exception_context();
    return TRUE;
}
#endif
