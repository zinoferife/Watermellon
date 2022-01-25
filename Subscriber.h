#pragma once
#include <iostream>
#include <spdlog/spdlog.h>
#include "Client.h"


//listens for messages that the publisher publishes
//Query the server for information
//ask the publisher for data aswell

namespace wm {
	template<typename socket_type>
	class Subscriber : public Client, public std::enable_shared_from_this<Subscriber<socket_type>>
	{
	public:
		typedef socket_type socket_t;
		virtual ~Subscriber() {}
		virtual void deliver(const wm::Message& message) override {
			//deliver a message to the client that is connected as a subscriber,
			//might also deliver to the publisher, depending on the type of message

		}
		void SetPublisher(client_ptr pub) {
			mPublisher = pub;
		}

	private:
		wm::Message mReadMessage;
		client_ptr mPublisher;

	};
}
