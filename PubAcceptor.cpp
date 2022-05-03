#include "PubAcceptor.h"
using namespace wm;
PubAcceptor::PubAcceptor(asio::io_context& context, std::uint16_t port)
: Acceptor(context, port){
}

void PubAcceptor::HandleError(const asio::error_code& ec)
{
	spdlog::error("Publisher acceptor error: {}: {:d}", ec.message(), ec.value());
}

void PubAcceptor::OnAccept(const asio::error_code& ec, std::shared_ptr<asio::ip::tcp::socket> socket)
{
	if (ec) {
		HandleError(ec);
		return;
	}
	auto pub = std::make_shared<wm::Publisher>(socket);
	if (pub) {
		pub->Start();
		spdlog::info("Publisher connected on {}:{:d}", socket->remote_endpoint().address().to_string(),
			socket->remote_endpoint().port());
		//supposed to add to the servers pub list
		if (!mStopFlag.load()) {
			//schedule another accept;
			InitAccept();
		}
	}
}
