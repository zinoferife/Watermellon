#include "Client.h"
std::shared_timed_mutex wm::mActivePublisherMutex{};
std::unordered_map<boost::uuids::uuid, wm::client_ptr> wm::mActivePublishers{};
std::unordered_map<boost::uuids::uuid, std::vector<wm::client_ptr>> wm::mPendingSubscribers{};
std::shared_timed_mutex wm::mPendingSubscribersMutex{};


void wm::Client::GenerateID()
{
	mClientId = boost::uuids::random_generator_mt19937()();
}
