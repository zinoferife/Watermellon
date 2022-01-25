#include "Server.h"
wm::Server::Server(asio::io_context& context, asio::ip::tcp::endpoint& endpoint)
: mContext(context){
	mPubAcceptor = std::make_unique<PubAcceptor>(context, 3030);
	if (!mPubAcceptor) {
		spdlog::error("Cannot create a publisher acceptor");
		return;
	}
	mPubAcceptor->Start();
}
