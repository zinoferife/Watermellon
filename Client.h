#pragma once
//base class for the types of clients that watermellon can connet to 
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include "Message.h"
#include <mutex>
#include <shared_mutex>
#include <nlohmann/json.hpp>

namespace wm {
	class Client
	{
	public:
		inline void SetName(const std::string& name) { mClientName = name; }
		inline void SetId(const boost::uuids::uuid& id) { mClientId = id; }
		inline const std::string& GetName() const { return mClientName; }
		inline const boost::uuids::uuid& GetClientID() const { return mClientId; }
		inline const std::string GetIdAsString() const {
			return boost::uuids::to_string(mClientId);
		}
		void GenerateID();
		virtual ~Client() {}
		virtual void deliver(const Message& message) = 0;
		virtual void read() = 0;
		virtual const js::json& get_info() const = 0;
		virtual void send() {}
 	protected:
		std::string mClientName;
		boost::uuids::uuid mClientId;
	};
	typedef std::shared_ptr<Client> client_ptr;
	extern std::unordered_map<boost::uuids::uuid,client_ptr> mActivePublishers;
	extern std::shared_timed_mutex mActivePublisherMutex;
	extern std::unordered_map<boost::uuids::uuid, std::vector<client_ptr>> mPendingSubscribers;
	extern std::shared_timed_mutex mPendingSubscribersMutex;

	//for sets 
	inline bool operator<(const client_ptr& rhs, const client_ptr& lhs) {
		return ((*rhs).GetClientID() < (*lhs).GetClientID());
	}
}

namespace std {
	template<>
	struct hash<boost::uuids::uuid>
	{
		size_t operator()(const boost::uuids::uuid& uuid) const noexcept
		{
			return boost::uuids::hash_value(uuid);
		}
	};
}
