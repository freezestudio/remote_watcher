//
// common head files
//
#ifndef COMMON_DEP_H
#define COMMON_DEP_H

// Windows

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>

// STL

#include <string>
#include <format>
#include <filesystem>

namespace fs = std::filesystem;

// track
#define ALLOW_TRACK

#define SERVICE_NAME L"rgmsvc"

#if defined(ALLOW_TRACK)
#ifndef DEBUG_STRING
#define DEBUG_STRING(msg, ...) \
do { \
    auto _msg_ = std::format(msg, __VA_ARGS__); \
    OutputDebugString(_msg_.c_str()); \
}while(false)
#endif
#else
#if defined(DEBUG) || defined(_DEBUG)
#ifndef DEBUG_STRING
#define DEBUG_STRING(msg, ...) \
do { \
    auto _msg_ = std::format(msg, __VA_ARGS__); \
    OutputDebugString(_msg_.c_str()); \
}while(false)
#endif
#else
#ifndef DEBUG_STRING
#define DEBUG_STRING(msg, ...)
#endif
#endif
#endif

#endif
