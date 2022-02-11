#include "service_thread_nats.h"
#include "service_extern.h"

namespace freeze
{
	void maybe_send_message(nats_client &nc)
	{
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_message() ...\n");
		nc.notify_message();
	}

	void maybe_send_payload(nats_client &nc, fs::path const &root)
	{
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_send_payload() ...\n");
		nc.notify_payload(root);
	}

	void maybe_response_command(nats_client &nc)
	{
		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command(): ...\n");

		g_cmd_response = {};
		nc.notify_command();

		// error: want wait, but failure.
		// g_cmd_response_signal.wait();

		// test only
		// char msg[256] = {};
		// sprintf_s(msg, "@rg Service-Thread-nats: maybe_response_command(): g_cmd_response_signal wait ready, response=%s, or null\n", g_cmd_response.c_str());
		// OutputDebugStringA(msg);

		// if (g_cmd_response == CMD_FOLDER)
		// {
		// 	DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command(): try reset work folder...\n");
		// 	reset_work_folder(true);
		// }
		// else if (g_cmd_response == CMD_IGNORE)
		// {
		// }
		// else
		// {
		// 	// empty.
		// }

		DEBUG_STRING(L"@rg Service-Thread-nats: maybe_response_command(): done.\n");
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
