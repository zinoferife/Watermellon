#include "SubAcceptor.h"

wm::SubAcceptor::SubAcceptor(asio::io_context& context, std::uint16_t port)
: Acceptor(context, port){

}

void wm::SubAcceptor::HandleError(const asio::error_code& ec)
{
	spdlog::error("Subscriber acceptor error: {}: {:d}", ec.message(), ec.value());
}

void wm::SubAcceptor::OnAccept(const asio::error_code& ec, std::shared_ptr<asio::ip::tcp::socket> socket)
{
	if (ec) {
		HandleError(ec);
		return;
	}
	auto sub = std::make_shared<wm::Subscriber<asio::ip::tcp::socket>>(socket);
	if (sub) {
		spdlog::info("Subscriber connected on {}:{:d}", socket->remote_endpoint().address().to_string(),
			socket->remote_endpoint().port());
		if (!mStopFlag.load()) {
			InitAccept();
		}
	}
}
