#ifndef UTILS_H
#define UTILS_H

#ifdef _DEBUG
template<typename ...Args>
void debug_output(std::wstring_view const&, Args&& ...);
#else
#define debug_output(...)
#endif

#endif
