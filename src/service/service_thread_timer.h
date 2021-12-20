#ifndef SERVICE_THREAD_TIMER_H
#define SERVICE_THREAD_TIMER_H

#include "common_dep.h"
#include "service_dep.h"

// timer thread callback
void __stdcall _TimerCallback(LPVOID, DWORD, DWORD);

namespace freeze
{
    class rgm_timer
    {
    public:
        rgm_timer();
        ~rgm_timer();

    public:
        void start();
        void stop();
        void pause();
        void resume();

    private:
        std::thread _thread;

    private:
        bool _running{false};
        bool _paused{false};
    };
}

#endif
