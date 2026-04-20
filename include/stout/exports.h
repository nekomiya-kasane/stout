#pragma once

#if defined(STOUT_BUILD_INTERNAL)
    #if defined(_MSC_VER)
        #define STOUT_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define STOUT_API __attribute__((visibility("default")))
    #else
        #define STOUT_API
    #endif
#else
    #if defined(_MSC_VER)
        #define STOUT_API __declspec(dllimport)
    #else
        #define STOUT_API
    #endif
#endif
