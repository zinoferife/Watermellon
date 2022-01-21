#pragma once
//base class for the types of clients that watermellon can connet to 
#include <boost/uuid/uuid.hpp>
#include <string>
#include <memory>

namespace wm {
	class Client
	{
	public:
		virtual ~Client() {}
		virtual void deliver() = 0;

	protected:
		std::string mClientName;
		boost::uuids::uuid mClientId;
	};
	typedef std::shared_ptr<Client> client_ptr;
}