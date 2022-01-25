#pragma once
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <vector>
#include <functional>
#include <atomic>



//set up an acceptor on different ports for different types of clients
//base class for different types of acceptors
//tcp acceptor, 
namespace wm {
	class Acceptor
	{
	public:
		constexpr static int max_listen_log = 100;
		Acceptor(asio::io_context& contex, std::uint16_t port);
		void InitAccept();
		void Start();
		void Stop();
		void Close();
	protected:
		virtual void HandleError(const asio::error_code& ec);
		virtual void OnAccept(const asio::error_code& ec, std::shared_ptr<asio::ip::tcp::socket> socket);
	protected:
		std::atomic_bool mStopFlag;
		asio::io_context& mIoService;
		asio::ip::tcp::acceptor mAcceptor;

	};
}