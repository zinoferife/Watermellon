#include "Publisher.h"

wm::Publisher::~Publisher()
{
	std::unique_lock<std::shared_timed_mutex> lock(mActivePublisherMutex);
	mActivePublishers.erase(GetClientID());
}

wm::Publisher::Publisher(std::shared_ptr<asio::ip::tcp::socket> socket)
: mSocket(socket), mInitialised(false){

}

void wm::Publisher::InitPublisher()
{
	auto& json = mReadMessage.GetJsonMessage();
	try {
		if (json["Type"] == "Init") {
			if (json["Pub_id"] == boost::uuids::to_string(boost::uuids::nil_uuid())) {
				//generate pub id for this client new client
				GenerateID();
			}
			else {
				boost::uuids::uuid id = boost::uuids::string_generator()(std::string(json["Pub_id"]));
				if (id != boost::uuids::nil_uuid()) {
					SetId(id);
				} else {
					spdlog::error("Publisher ID invalid");
					wm::Message message;
					message.GetJsonMessage() = {
						{"Type", "Error"},
						{"Code", 102},
						{"Message", "Publisher id is invalid, resend init message with \"Pub_id = nil_id\" to generate new ID"}
					};
					deliver(message);
					return;
				}
			}
			std::unique_lock<std::shared_timed_mutex> lock(mActivePublisherMutex);
			auto self = this->shared_from_this();
			auto [iter, inserted] = mActivePublishers.insert({ GetClientID(), self });
			lock.unlock();
			if (!inserted) {
				spdlog::error("Publisher already exisit, closing this publisher");
				wm::Message message;
				message.GetJsonMessage() = {
					{"Type", "Error"},
					{"Code", 100},
					{"Message", "Publisher Already exists"}
				};
				deliver(message);
				//shut down the client socket on both recieveing and sending messages
				//mSocket->shutdown(typename socket_t::shutdown_both);
				return;
			}
			auto name = std::string(json["Name"]);
			if (name.empty()) {
				SetName("Default Publisher name");
			}
			else {
				SetName(json["Name"]);
			}
			ProcessPendingSubscribers();
		} else {
			spdlog::error("Publisher not initialised, Expected an Init message from the client");
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
		spdlog::error("Json format error in Publisher, {}", error.what());
		wm::Message message;
		message.GetJsonMessage() = {
			{"Type", "Error"},
			{"Code", 111},
			{"Message", "Json format error, Json message was not in the correct schema"}
		};
		deliver(message);
		return;
	}
	//properly initialised the publisher
	wm::Message message;
	message.GetJsonMessage() = {
		{"Type", "Init"},
		{"Code", 200},
		{"Message", "Sucessfully initialised the publisher"},
		{"ID", boost::uuids::to_string(GetClientID())},
		{"Name", GetName()}
	};
	deliver(message);
	mInitialised = true;
}

void wm::Publisher::deliver(const wm::Message& message)
{
	//deliver message to the clients that are connected as publisher,
	bool write_in_progress = !mMessageStrings.empty();
	auto data = message.GetJsonAsString();
	data += "\r\n";
	mMessageStrings.push_back(data);
	if (!write_in_progress) {
		do_write();
	}
}

void wm::Publisher::read()
{
	//read a message from the client, 
	do_read();
}

void wm::Publisher::send()
{

}

void wm::Publisher::Start()
{
	//read the data from the client
	read();
}

void wm::Publisher::Close()
{
	ClosePublisher();
}

void wm::Publisher::ShutDown()
{
	//shut down socket
	fmt::print("Shuting down publisher {}", GetIdAsString());
	wm::Message m;
	m.GetJsonMessage() = {
		{"Type", "ShutDown"}
	};
	for (auto sub : mSubscibers) {
		sub.second->deliver(m);
		sub.second->RemovePublisher();
	}
	mSubscibers.clear();
	
	std::unique_lock<std::shared_timed_mutex> Lock(mActivePublisherMutex);
	auto iter = mActivePublishers.find(GetClientID());
	if (iter != mActivePublishers.end()) {
		mActivePublishers.erase(iter);
	}
	ClosePublisher();
}

void wm::Publisher::AddSubscriber(client_ptr subsriber)
{
	auto [iter, inserted] = mSubscibers.insert({ subsriber->GetClientID(), std::dynamic_pointer_cast<Subscriber, Client>(subsriber) });
	if (inserted) {
		subsriber->deliver(ComposeWelcomeMessage());
	}
}

void wm::Publisher::InitMessageHandlers()
{
	AddMessageHandlers("Submesg", [](const wm::Message& message, client_ptr client) {
		auto pub = std::dynamic_pointer_cast<Publisher, Client>(client);
		pub->OnSubMessage(message);
	});
}

void wm::Publisher::OnSubMessage(const wm::Message& message)
{
	auto& json = message.GetJsonMessage();
	std::string subid = json["Sub_id"];
	spdlog::info("Sending to {}", subid);
	boost::uuids::uuid id = boost::uuids::string_generator()(subid);
	auto iter = mSubscibers.find(id);
	if (iter != mSubscibers.end()) {
		(*iter).second->deliver(message);
	}
}

void wm::Publisher::OnServerMessage(const wm::Message& message)
{
}

void wm::Publisher::do_write()
{
	auto self = this->shared_from_this();
	asio::async_write(*mSocket, asio::buffer(mMessageStrings.front().data(), mMessageStrings.front().length()),
		[this, self](const asio::error_code& ec, size_t bytes) {
			OnWrite(ec, bytes);
		});
}

void wm::Publisher::do_read()
{
	//read a message from the publisher client,
			//schedle a read on the socket
	auto self = this->shared_from_this();
	asio::async_read_until(*mSocket, mReadMessage.GetStreamBuffer(), "\r\n", [this, self](const asio::error_code& ec, size_t bytes) {
		OnRead(ec, bytes);
		});
}

void wm::Publisher::ProcessMessage()
{

}

bool wm::Publisher::verifyJsonObject(js::json& json)
{
	return true;
}

void wm::Publisher::ClosePublisher()
{

}

wm::Message wm::Publisher::ComposeWelcomeMessage()
{
	Message m;
	m.GetJsonMessage() = {
		{"Type", "WELCOMESUB"},
		{"Name", GetName()},
		{"Pub_id", GetIdAsString()},
		{ "Configuration",
			{
				{"version", "0.0.1"}
			}
		},
		{"PubInfo", mPublisherInformation}
	};
	return m;
}

void wm::Publisher::ProcessPendingSubscribers()
{
	std::shared_lock<std::shared_timed_mutex> lock(mPendingSubscribersMutex);
	auto iter = mPendingSubscribers.find(GetClientID());
	if (iter != mPendingSubscribers.end()) {
		auto& vector = iter->second;
		for (auto sub : vector) {
			auto [iter, inserted] = mSubscibers.insert({ sub->GetClientID(),
				std::dynamic_pointer_cast<Subscriber, Client>(sub) });
			//ignore if  already inserted
			if (inserted) {
				auto self = this->shared_from_this();
				iter->second->SetPublisher(self);
				sub->deliver(ComposeWelcomeMessage());
				iter->second->WriteBacklog();
			}
		}
		lock.unlock();
		{
			std::unique_lock<std::shared_timed_mutex> uLock(mPendingSubscribersMutex);
			mPendingSubscribers.erase(GetClientID());
		}
	}
}

void wm::Publisher::OnRead(const asio::error_code& ec, size_t bytes_read)
{
	if (!ec) {
		//check message 
		//log message header
		if (mReadMessage.ParseStreamBuffer()) {
			if (!mInitialised) {
				InitPublisher();
			}
			else {
				auto& json = mReadMessage.GetJsonMessage();
				if (!json.is_null()) {
					auto iter = mMessageHandlers.find(json["Type"]);
					if (iter != mMessageHandlers.end()) {
						iter->second(mReadMessage, shared_from_this());
					}
					else {
						spdlog::error("No hanlder for {}", json["Type"]);
					}
				}
				
			}
		}
		this->do_read();
	}
	else if (ec.value() == asio::error::shut_down) {
		//connetion shut down by the client that is connected as a publisher
		//initiate a close down of the socket and the inform the subscribers that 
		//the publisher if offline... 
		ShutDown();
		spdlog::error("Could not read from publisher, publisher shut down {}:{:d}, {}", mSocket->remote_endpoint().address().to_string(),
			mSocket->remote_endpoint().port(), boost::uuids::to_string(GetClientID()));

	}

	else {
		//error log error
		ShutDown();
		spdlog::error("Could not read from publisher, publisher error {}:{:d}, {}", mSocket->remote_endpoint().address().to_string(),
			mSocket->remote_endpoint().port(), boost::uuids::to_string(GetClientID()));
	}
}

void wm::Publisher::OnWrite(const asio::error_code& ec, size_t bytes)
{
	if (!ec) {
		//log message write status
		mMessageStrings.pop_front();
		if (!mMessageStrings.empty()) {
			do_write();
		}
		return;
	}
	else if (ec.value() == asio::error::shut_down) {
		//connection closed. initiate a close operation on the publisher 
		ShutDown();
		spdlog::error("Could not write to publisher, publisher shut down {}:{:d}, {}", mSocket->remote_endpoint().address().to_string(),
			mSocket->remote_endpoint().port(), boost::uuids::to_string(GetClientID()));
		//mSocket->close();
	}
	else {
		spdlog::error("Could not write to publisher {}:{:d}, {}", mSocket->remote_endpoint().address().to_string(),
			mSocket->remote_endpoint().port(), boost::uuids::to_string(GetClientID()));
	}
}
