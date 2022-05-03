#include "Server.h"
wm::Server::Server(asio::io_context& context)
: mContext(context){
	mWork.reset(new asio::io_context::work(context));
	Client::InitMessageHandlers();
	Publisher::InitMessageHandlers();
	Subscriber::InitMessageHandlers();
}

void wm::Server::AddAcceptor(std::unique_ptr<Acceptor>&& acceptor)
{
	//an idea to have a vector of acceptors 
}

void wm::Server::StopServer()
{
	spdlog::info("Stoping watermellon...");
	mPubAcceptor->Stop();
	mSubAcceptor->Stop();
	mContext.stop();
	for (auto& th : mthreadPool) {
		th->join();
	}
	spdlog::info("Watermellon stopped.");
}

void wm::Server::StartServer()
{
	spdlog::info("Starting watermellon...");
	mPubAcceptor = std::make_unique<PubAcceptor>(mContext, 3030);
	if (!mPubAcceptor) {
		spdlog::error("Cannot create a publisher acceptor");
		return;
	}

	mSubAcceptor = std::make_unique<SubAcceptor>(mContext, 3033);
	if (!mSubAcceptor) {
		spdlog::error("Cannot create a subscriber acceptor");
		return;
	}
	mPubAcceptor->Start();
	mSubAcceptor->Start();

	//create the thread pool 
	for (std::uint32_t i = 0; i < max_thread; i++) {
		mthreadPool.push_back(std::make_unique<std::thread>([this]() {
			mContext.run();
		}));
	}
}
