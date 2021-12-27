#include "service_thread_nats.h"
#include "service_extern.h"

namespace freeze
{
	void maybe_send_message(nats_client& nc)
	{
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_message() ...\n");
		nc.notify_message();
	}

	void maybe_send_payload(nats_client& nc, fs::path const& root)
	{
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_payload() ...\n");
		nc.notify_payload(root);
	}

	void maybe_response_command(nats_client& nc)
	{
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command(): ...\n");
		auto cmd = nc.notify_command();
		if (cmd == "modify-folder")
		{
			reset_work_folder(true);
		}
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command(): done.\n");
	}
}

namespace freeze
{
	rgm_nats::rgm_nats()
		: _signal{}
		, _signal_reason{}
	{

	}

	rgm_nats::~rgm_nats()
	{

	}

	void rgm_nats::start()
	{

	}

	void rgm_nats::stop()
	{

	}

	void rgm_nats::pause()
	{

	}

	void rgm_nats::resume()
	{

	}
}
