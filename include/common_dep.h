//
// common head files
//
#ifndef COMMON_DEP_H
#define COMMON_DEP_H

// Windows

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// STL

#include <string>
#include <format>
#include <filesystem>

#define SERVICE_NAME L"rgmsvc"

//#if defined(DEBUG) || defined(_DEBUG)
#define DEBUG_STRING(msg, ...) \
do { \
    auto _msg_ = std::format(msg, __VA_ARGS__); \
    OutputDebugString(_msg_.c_str()); \
}while(false)
//#else
//#define DEBUG_STRING(msg, ...)
//#endif

#endif
