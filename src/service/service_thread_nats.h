#ifndef SERVICE_THREAD_NATS_H
#define SERVICE_THREAD_NATS_H

#include "service_nats_client.h"

namespace freeze
{
    void maybe_send_message(nats_client const&);
    void maybe_send_payload(nats_client const&, fs::path const&);
}

namespace freeze
{
    class rgm_nats
    {
    public:
        rgm_nats();
        ~rgm_nats();

    public:
        void start();
        void stop();
        void pause();
        void resume();

    private:
        std::thread _thread;

    private:
        bool _running{ false };
        bool _paused{ false };
    };
}

#endif
