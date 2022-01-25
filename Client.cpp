#include "Client.h"
std::unordered_map<boost::uuids::uuid,wm::client_ptr> wm::mActivePublishers;

void wm::Client::GenerateID()
{
	mClientId = boost::uuids::random_generator_mt19937()();
}
