#include "Subscriber.h"
#include "Publisher.h"


wm::Subscriber::Subscriber(std::shared_ptr<asio::ip::tcp::socket> socket):
	mSocket(socket), mInitialised(false)
{
	
}

void wm::Subscriber::deliver(const wm::Message& message)
{
	//deliver a message to the client that is connected as a subscriber
	bool write_in_progress = !mMessageStrings.empty();
	auto data = message.GetJsonAsString();
	data += "\r\n";
	mMessageStrings.push_back(data);
	if (!write_in_progress) {
		do_write();
	}
}

void wm::Subscriber::InitSubscriber()
{
	auto& json = mReadMessage.GetJsonMessage();
	try {
		if (json["Type"] == "Init") {
			if (json["Sub_id"] == boost::uuids::to_string(boost::uuids::nil_uuid())) {
				GenerateID();
			}
			else {
				boost::uuids::uuid id = boost::uuids::string_generator()(std::string(json["Sun_id"]));
				if (id != boost::uuids::nil_uuid()) {
					SetId(id);
				} else {
					spdlog::error("Subsriber ID is invalid");
					wm::Message message;
					message.GetJsonMessage() = {
						{"Type", "Error"},
						{"Code", 105},
						{"Message", "Subscriber ID is invalid,  resend init message with \"Sub_id = nil_id\" to generate new ID"}
					};
					deliver(message);
					return;
				}
			}

			boost::uuids::uuid id = boost::uuids::string_generator()(std::string(json["Publisher_id"]));
			if (id != boost::uuids::nil_uuid()) {
				std::shared_lock<std::shared_timed_mutex> lock(mActivePublisherMutex);
				auto iter = mActivePublishers.find(id);
				if (iter != mActivePublishers.end()) {
					SetPublisher(iter->second);
				}
				else {
					spdlog::info("{} publisher not online", boost::uuids::to_string(id));
					wm::Message message;
					message.GetJsonMessage() = {
					{"Type", "Error"},
					{"Code", 301},
					{"Message", fmt::format("{} is not online", boost::uuids::to_string(id))}
					};
					deliver(message);
				}
			} else {
				//invalid or null publisher id,
				//	send the list of active publishers to the client to choose publisher
				SendListOfActivePublishers();
				return;
			}
					//contine with the init process, store subscriber in the
					//pending subsciber list, awaiting the publisher to come online
			{
					std::unique_lock<std::shared_timed_mutex> uLock(mPendingSubscribersMutex);
					auto iter = mPendingSubscribers.find(id);
					if (iter == mPendingSubscribers.end()) {
							std::vector<client_ptr> subs;
							subs.push_back(this->shared_from_this());
							auto [iter, inserted] = mPendingSubscribers.insert({ id, std::move(subs) });
					} else {
							iter->second.push_back(this->shared_from_this());
					}
			}
			//initalised
			auto name = std::string(json["Name"]);
			if (name.empty()) {
				SetName("Default Subscriber name");
			}
			else {
				SetName(json["Name"]);
			}

			wm::Message message;
			message.GetJsonMessage() = {
				{"Type", "Init"},
				{"Code", 200},
				{"Message", "Sucessfully initialised the subscriber"},
				{"ID", boost::uuids::to_string(GetClientID())},
				{"Name", GetName()}
			};
			deliver(message);
			InformPublisher();
			mInitialised = true;
		}
		else {
			spdlog::error("Subscriber not initialised, Expected an Init message from the client");
			wm::Message message;
			message.GetJsonMessage() = {
				{"Type", "Error"},
				{"Code", 101},
				{"Message", "Expected an init message from client, invalid message sent"}
			};
			deliver(message);
			return;
		}
	}
	catch (js::json::type_error& error) {
		spdlog::error("Json format error in Subsciber, {}", error.what());
		wm::Message message;
		message.GetJsonMessage() = {
			{"Type", "Error"},
			{"Code", 111},
			{"Message", "Json format error, Json message was not in the correct schema"}
		};
		deliver(message);
		return;
	}

}

void wm::Subscriber::RemovePublisher()
{
	if (mPublisher) {
		mPublisher.reset();
	}
}

void wm::Subscriber::SendListOfActivePublishers()
{
	std::shared_lock<std::shared_timed_mutex> lock(mActivePublisherMutex);
	js::json pubList = js::json::array();
	for (auto& activeIter : mActivePublishers) {
		js::json pub;
		pub["Publisher_id"] = boost::uuids::to_string(activeIter.first);
		pub["Publisher_name"] = activeIter.second->GetName();
		//send info too ? ?
		pub["Publisher_info"] = activeIter.second->get_info();
		pubList.push_back(pub);
	}
	wm::Message message;
	message.GetJsonMessage() = {
		{"Type", "Publist"},
		{"Publishers", pubList}
	};
	deliver(message);
}

void wm::Subscriber::read()
{
	do_read();
}

void wm::Subscriber::send()
{
	//do nothing yet
}

void wm::Subscriber::Start()
{
	read();
}

void wm::Subscriber::Close()
{
	//remove from pending if in pending
	std::unique_lock<std::shared_timed_mutex> lock(mPendingSubscribersMutex);
	mPendingSubscribers.erase(GetClientID());
	mSocket->close();
}

void wm::Subscriber::ShutDown()
{
}

void wm::Subscriber::WriteBacklog()
{
	for (const auto& message : mMessageBacklog) {
		mPublisher->deliver(message);
	}
}

void wm::Subscriber::DeleteSubscriber()
{
	//check if subscibers is in the pending list;
	std::shared_lock<std::shared_timed_mutex> lock(mPendingSubscribersMutex);
	for (auto& SubVectorIter : mPendingSubscribers) {
		for (int i = 0; i < SubVectorIter.second.size(); i++) {
			auto sub = std::next(SubVectorIter.second.begin(), i);
			if ((*sub)->GetClientID() == GetClientID()) {
				lock.unlock();
				std::unique_lock<std::shared_timed_mutex> uLock(mPendingSubscribersMutex);
				SubVectorIter.second.erase(sub);
				break;
			}
		}
	}
	if (lock.owns_lock()) lock.unlock();

	//cancel all pending operations on the subscriber
	asio::error_code ec;
	mSocket->cancel(ec);
	if (ec) {
		spdlog::error("Failed to cancel operations on {}", GetIdAsString());
		return;
	}
}

void wm::Subscriber::HandleError(const asio::error_code& ec)
{
	if (ec.value() == asio::error::eof) {
		spdlog::info("{} is shutdown, connection dropped", boost::uuids::to_string(GetClientID()));
		DeleteSubscriber();
		return;
	}
	spdlog::error("Subsciber {} I/O error {}, {:d}", boost::uuids::to_string(GetClientID()), ec.message(), ec.value());
	DeleteSubscriber();
}

void wm::Subscriber::InformPublisher()
{

	if (mPublisher) {
		wm::Message messge;
		messge.GetJsonMessage() = {
			{"Type", "Subscribe"},
			{"Message", "Subscribed to publisher"},
			{"Sub_id", GetIdAsString()}
		};
		mPublisher->deliver(messge);
		auto pub = std::dynamic_pointer_cast<Publisher, Client>(mPublisher);
		pub->AddSubscriber(shared_from_this());
	}
}

void wm::Subscriber::do_write()
{
	auto self = shared_from_this();
	asio::async_write(*mSocket, asio::buffer(mMessageStrings.front().data(), mMessageStrings.front().length()), [this, self](const asio::error_code& ec, size_t bytes) {
		OnWrite(ec, bytes);
	});
}

void wm::Subscriber::do_read()
{
	//read from the client
	auto self = shared_from_this();
	asio::async_read_until(*mSocket, mReadMessage.GetStreamBuffer(), "\r\n", [this, self](const asio::error_code& ec, size_t bytes) {
		OnRead(ec, bytes);
	});
}

void wm::Subscriber::OnWrite(const asio::error_code& ec, size_t bytes)
{
	if (ec) {
		//error in writing
			spdlog::error("{}: {}:{:d}", boost::uuids::to_string(GetClientID()), ec.message(), ec.value());
	}
	else {
		mMessageStrings.pop_front();
		if (!mMessageStrings.empty()) {
			do_write();
		}
	}
}

void wm::Subscriber::OnRead(const asio::error_code& ec, size_t bytes)
{
	if (!ec) {
		if (mReadMessage.ParseStreamBuffer()) {
			if (!mInitialised) {
				InitSubscriber();
			}
			else {
				auto& json = mReadMessage.GetJsonMessage();
				auto iter = mMessageHandlers.find(json["Type"]);
				if (iter != mMessageHandlers.end()) {
					(iter->second(mReadMessage, shared_from_this()));
				}
				else {
					spdlog::error("{} subscriber: Invalid Message type: {}", boost::uuids::to_string(GetClientID()),
						json["Type"]);
				}
			}
		}
		//schedule aother read operation
			do_read();
	}
	else {
		HandleError(ec);
	}
}

void wm::Subscriber::InitMessageHandlers()
{
	AddMessageHandlers("Pubmesg", [](const wm::Message& message, client_ptr client) {
		auto sub = std::dynamic_pointer_cast<Subscriber, Client>(client);
		sub->PublisherMesssageHandler(message);
	});
	AddMessageHandlers("Servermesg", [](const wm::Message& message, client_ptr client) {
		auto sub = std::dynamic_pointer_cast<Subscriber, Client>(client);
		sub->ServerMessageHandler(message);
	});

}

void wm::Subscriber::PublisherMesssageHandler(const wm::Message& message)
{
	if (mPublisher) {
		mPublisher->deliver(message);
	}
	else {
		mMessageBacklog.push_back(message);
	}
}

void wm::Subscriber::ServerMessageHandler(const wm::Message& message)
{

}
