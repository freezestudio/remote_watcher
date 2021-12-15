#include "service_thread_nats.h"

namespace freeze
{
    void maybe_send_data(fs::path const& folder, nats_client& nc)
    {
		auto& vec_changes = freeze::detail::get_changed_information();
		nc.notify_payload(folder, vec_changes);
    }
}
