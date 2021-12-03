#ifndef UTILS_H
#define UTILS_H

#ifdef _DEBUG

#include <iostream>
using namespace std::literals;

template <typename... Args>
void debug_output(std::wstring_view const &fmt, Args &&...args)
{
    auto msg = std::format(fmt, args...);
    
    // test only
    // std::wcout << msg << std::endl;

    OutputDebugString(msg.data());
}

#else
#define debug_output(...)
#endif

#endif
