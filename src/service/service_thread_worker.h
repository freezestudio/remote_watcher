#ifndef SERVICE_THREAD_WORKER_H
#define SERVICE_THREAD_WORKER_H

// #include "service_watch.h"
#include "service_watch_ex.h"

namespace freeze
{
    class rgm_worker
    {
    public:
        rgm_worker();
        ~rgm_worker();

    public:
        void start();
        void stop();
        void pause();
        void resume();

    private:
        std::thread _thread;
        atomic_sync _signal;

    private:
        bool _running;
        bool _paused;
    };
}

#endif
