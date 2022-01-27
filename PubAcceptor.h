#pragma once
#include "Acceptor.h"
#include "Publisher.h"

namespace wm {
	class PubAcceptor : public Acceptor
	{
	public:
		PubAcceptor(asio::io_context& context, std::uint16_t port);

	protected:
		virtual void HandleError(const asio::error_code& ec) override;
		virtual void OnAccept(const asio::error_code& ec, std::shared_ptr<asio::ip::tcp::socket> socket) override;

	};
}