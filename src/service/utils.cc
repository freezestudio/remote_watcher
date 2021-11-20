#include "dep.h"
#include "utils.h"

template<typename... Args>
void debug_output(std::wstring_view const& fmt, Args&&... args)
{
    auto msg = std::format(fmt, args...);
    OutputDebugString(msg.data());
}
