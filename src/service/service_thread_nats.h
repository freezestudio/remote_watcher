#ifndef SERVICE_THREAD_NATS_H
#define SERVICE_THREAD_NATS_H

#include "service_nats_client.h"

namespace freeze
{
    void maybe_send_message(nats_client&);
    void maybe_send_payload(nats_client&, fs::path const&);
    void maybe_response_command(nats_client&);
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
        atomic_sync _signal;
        atomic_sync_reason _signal_reason;

    private:
        bool _running{ false };
        bool _paused{ false };
    };
}

#endif
