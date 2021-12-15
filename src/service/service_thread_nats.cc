#include "service_thread_nats.h"

namespace freeze
{
    //std::mutex folder_info_mutex;
    void maybe_send_data(fs::path const& folder, nats_client& nc)
    {
        OutputDebugString(L"@rg Service-Thread-NATS: maybe_send_data() ...\n");
        //std::lock_guard<std::mutex> lock(folder_info_mutex);
		auto vec_changes = freeze::detail::get_changed_information();
		nc.notify_payload(folder, vec_changes);
    }
}
