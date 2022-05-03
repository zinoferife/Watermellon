#pragma once
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <vector>
#include <thread>

#include  "Client.h"
#include "Acceptor.h"
#include "PubAcceptor.h"
#include "SubAcceptor.h"



namespace wm {
	class Server
	{
	public:
		constexpr static int max_thread = 10;
		Server(asio::io_context& context);
		void AddAcceptor(std::unique_ptr<Acceptor>&& acceptor);
		void StopServer();
		void StartServer();
	private:
		asio::io_context& mContext;
		std::unique_ptr<asio::io_service::work> mWork;
		std::unique_ptr<Acceptor> mPubAcceptor;
		std::unique_ptr<Acceptor> mSubAcceptor;
		std::vector<std::unique_ptr<std::thread>>  mthreadPool;
	};
}
