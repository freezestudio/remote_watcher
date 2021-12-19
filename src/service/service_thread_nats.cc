#include "service_thread_nats.h"

namespace freeze
{
    void maybe_send_message(nats_client const& nc)
    {

    }

    void maybe_send_payload(nats_client const& nc, fs::path const& root)
    {
        DEBUG_STRING(L"@rg Service-Thread-NATS: maybe_send_payload() ...\n");
		nc.notify_payload(root);
    }
}
