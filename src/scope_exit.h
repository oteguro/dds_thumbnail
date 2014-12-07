// ----------------------------------------------------------------------------
// scope_exit.h
// ----------------------------------------------------------------------------
// Description : Tiny "ScopeExit" on C++11. 
#pragma once

template <typename T> class ScopeExit
{
public:
    T m_func;

    ScopeExit(T func)
        : m_func(func)
    {
    }
    ~ScopeExit()
    {
        m_func();
    }

}; // class ScopeExit
    
template <typename T> ScopeExit<T> MakeScopeExit(T func)
{
    return ScopeExit<T>(func);
}

#define SCOPE_EXIT_CONCAT_IMPL(x,y)     x ## y
#define SCOPE_EXIT_CONCAT(x,y)          SCOPE_EXIT_CONCAT_IMPL(x,y)
#define SCOPE_EXIT(code)                auto SCOPE_EXIT_CONCAT(scopeExit, __LINE__) = MakeScopeExit([&](){code;})

