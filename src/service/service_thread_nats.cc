#include "service_thread_nats.h"
#include "service_extern.h"

namespace freeze
{
	void maybe_send_message(nats_client &nc)
	{
		// async-send
		auto fut = std::async(std::launch::async, [&]()
							  {
			if (!nc.is_connected())
			{
				DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_message() nc not connected.\n");
				return;
			}
			DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_message() run ...\n");
			nc.notify_message();
			DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_message() done.\n"); });
	}

	void maybe_send_payload(nats_client &nc, fs::path const &root)
	{
		// auto fut = std::async(std::launch::async, [&]() {
		// 	if (!nc.is_connected())
		// 	{
		// 		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_payload() nc not connected.\n");
		// 		return;
		// 	}
		// 	DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_payload() run ...\n");
		// 	nc.notify_payload(root);
		// 	DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_payload() done.\n");
		// });

		// sync-send
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_payload() run ...\n");
		nc.notify_payload(root);
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_payload() done.\n");
	}

	void maybe_send_synfile(nats_client &nc)
	{
		// auto fut = std::async(std::launch::async, [&]()
		// 					  {
		// 	if (!nc.is_connected())
		// 	{
		// 		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_synfile() nc not connected.\n");
		// 		return;
		// 	}
		// 	DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_synfile() run ...\n");
		// 	nc.notify_synfiles();
		// 	DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_synfile() done.\n"); });

		// sync-send
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_synfile() run ...\n");
		nc.notify_synfiles();
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_synfile() done.\n");
	}

	void maybe_response_command(nats_client &nc)
	{
		// auto fut = std::async(std::launch::async, [&]()
		// 					  {
		// 	if (!nc.is_connected())
		// 	{
		// 		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command() nc not connected.\n");
		// 		return;
		// 	}
		// 	DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command() run ...\n");
		// 	g_cmd_response = {};
		// 	nc.notify_command();
		// 	DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command() done.\n"); });

		// sync-send
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command() run ...\n");
		g_cmd_response = {};
		nc.notify_command();
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command() done.\n");
	}
}

namespace freeze
{
	rgm_nats_channel::rgm_nats_channel()
		: _signal{}, _running{true}
	{
	}

	rgm_nats_channel::~rgm_nats_channel()
	{
	}

	void rgm_nats_channel::publish()
	{
	}

	void rgm_nats_channel::subject()
	{
	}

	void rgm_nats_channel::notify()
	{
	}
}

namespace freeze
{
	rgm_nats::rgm_nats()
		: _signal{}, _signal_reason{}
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
