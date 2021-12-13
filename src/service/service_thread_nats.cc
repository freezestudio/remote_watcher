#include "service_thread_nats.h"

namespace freeze
{
    void maybe_send_data(nats_client& nc)
    {
		auto& vec_changes = freeze::detail::get_changed_information();
		for (auto& d : vec_changes)
		{
			auto _msg = std::format(L"action={}, name={}\n"sv, d.action, d.filename);
			OutputDebugString(_msg.c_str());
		}
    }
}