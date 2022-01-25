#pragma once
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <vector>
#include  "Client.h"
#include "Acceptor.h"
#include "PubAcceptor.h"

namespace wm {
	class Server
	{
	public:
		Server(asio::io_context& context, asio::ip::tcp::endpoint& endpoint);
	private:
		asio::io_context& mContext;
		std::unique_ptr<asio::io_service::work> mWork;
		std::unique_ptr<Acceptor> mPubAcceptor;
		std::unique_ptr<Acceptor> mSubAcceptor;
		std::vector<std::unique_ptr<std::thread>>  mthreadPool;
	};
}
