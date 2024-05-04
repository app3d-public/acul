#ifndef APPLIB_API_H
#define APPLIB_API_H

#ifdef _WIN32
    #define API_EXPORT __declspec(dllexport)
    #define API_IMPORT __declspec(dllimport)
#else
    #define API_EXPORT __attribute__((visibility("default")))
    #define API_IMPORT __attribute__((visibility("default")))
#endif // _WIN32

#ifdef APPLIB_BUILD
    #define APPLIB_API API_EXPORT
#else
    #define APPLIB_API API_IMPORT
#endif

#endif // APPLIB_API_H