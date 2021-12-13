#ifndef SERVICE_THREAD_NATS_H
#define SERVICE_THREAD_NATS_H

#include "service_nats_client.h"

namespace freeze
{
    void maybe_send_data(nats_client&);
}

#endif
