#include "Client.h"
std::shared_timed_mutex wm::mActivePublisherMutex{};
std::unordered_map<boost::uuids::uuid, wm::client_ptr> wm::mActivePublishers{};
std::unordered_map<boost::uuids::uuid, std::vector<wm::client_ptr>> wm::mPendingSubscribers{};
std::shared_timed_mutex wm::mPendingSubscribersMutex{};
std::unordered_map<std::string, std::function<void(const wm::Message&, std::shared_ptr<wm::Client> client)>> wm::Client::mMessageHandlers;

void wm::Client::GenerateID()
{
	mClientId = boost::uuids::random_generator_mt19937()();
}

void wm::Client::OnError(const wm::Message& message)
{
	spdlog::error("{}", message.GetJsonMessage().dump(4));
}

//function needs to be called once on set up
void wm::Client::InitMessageHandlers()
{
	AddMessageHandlers("Error", [](const wm::Message& message, client_ptr client) {
		client->OnError(message);
	});
}

void wm::Client::AddMessageHandlers(const std::string& MesType, const std::function<void(const wm::Message&, std::shared_ptr<Client> client)>& function)
{
	auto [iter, inserted] = mMessageHandlers.insert({ MesType, function });
	if (!inserted && iter != mMessageHandlers.end()) {
		//assume that we are changing the message handlers
		iter->second = function;
	}
}
 