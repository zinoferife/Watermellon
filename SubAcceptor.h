#pragma once
#include "Acceptor.h"
#include "Subscriber.h"

namespace wm {
	class SubAcceptor : public Acceptor
	{
	public:
		SubAcceptor(asio::io_context& context, std::uint16_t port);

	protected:
		virtual void HandleError(const asio::error_code& ec) override;
		virtual void OnAccept(const asio::error_code& ec,
			std::shared_ptr<asio::ip::tcp::socket> socket) override;

	};
}

