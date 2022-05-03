#include "Acceptor.h"
using namespace wm;
Acceptor::Acceptor(asio::io_context& context, std::uint16_t port)
	:mIoService(context), 
	mAcceptor(context, asio::ip::tcp::endpoint{asio::ip::address_v4::any(), port})
{
	mStopFlag.store(false);
}

void Acceptor::InitAccept()
{
	std::shared_ptr<asio::ip::tcp::socket> Sock(new asio::ip::tcp::socket(mIoService));

	mAcceptor.async_accept(*Sock, [this, Sock](const asio::error_code& ec) {
		OnAccept(ec, Sock);
	});
}

void Acceptor::Start()
{
	asio::error_code ec;
	mAcceptor.listen(max_listen_log, ec);
	if(!ec){
		InitAccept();
	}
	else{
		//log cannot set acceptor in listen mode, error from somewhere
		mStopFlag.store(true);
	}
}

void Acceptor::Stop()
{
	if (!mStopFlag.load()) {
		mStopFlag.store(true);
	}
}

void wm::Acceptor::Close()
{
	//clost the socekt
	mAcceptor.close();
}

void Acceptor::HandleError(const asio::error_code& ec)
{
	//do nothing
}

void Acceptor::OnAccept(const asio::error_code& ec, std::shared_ptr<asio::ip::tcp::socket> socket)
{
	//also do nothing
}
