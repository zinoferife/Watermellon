#pragma once
/*
template<typename socket_type>
class Subscriber : public Client, public std::enable_shared_from_this<Subscriber<socket_type>>
{
public:
	typedef socket_type socket_t;
	Subscriber(std::shared_ptr<socket_type> socket) : mSocket(socket) {
		SetUpMessageHandlers();
		read();
	}
	virtual ~Subscriber() {}
	virtual void deliver(const wm::Message& message) override {
		//deliver a message to the client that is connected as a subscriber
		bool write_in_progress = !mMessageStrings.empty();
		auto data = message.GetJsonAsString();
		data += "\r\n";
		mMessageStrings.push_back(data);
		if (!write_in_progress) {
			do_write();
		}
	}

	void AddMessageHandler(const std::string& type,
		std::function<void(const wm::Message&)>&& function) {
		auto [iter, inserted] = mMessageHandlers.insert({ type, function });
		if (!inserted && iter != mMessageHandlers.end()) {
			//assume it already exist and that this is asking for a replacement on the handler
			iter->second = std::move(function);
		}
	}

	void SetUpMessageHandlers() {
		AddMessageHandler("Pubmessage", [this](const wm::Message& message) {
			PublisherMesssageHandler(message);
			});

	}

	void InitSubscriber() {
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
						id = boost::uuids::string_generator()(std::string(json["Publisher_id"]));
						if (id != boost::uuids::nil_uuid()) {
							{
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
									contine with the init process, store subscriber in the 
									pending subsciber list, awaiting the publisher to come online
									{
										std::unique_lock<std::shared_timed_mutex> uLock(mPendingSubscribersMutex);
										auto iter = mPendingSubscribers.find(id);
										if (iter == mPendingSubscribers.end()) {
											std::vector<client_ptr> subs;
											subs.push_back(this->shared_from_this());
											auto [iter, inserted] = mPendingSubscribers.insert({ id, std::move(subs) });
										}
										else {
											iter->second.push_back(this->shared_from_this());
										}
									}
								}
							}
						}
						else {
							invalid or null publisher id,
							send the list of active publishers to the client to choose publisher
							SendListOfActivePublishers();
							return;
						}
					}
					else {
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
				initalised
				SetName(json["Name"]);
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

	for now only send the list of active publishers, 
	later would have to find a way to read and send all publishers
	void SendListOfActivePublishers() {
		std::shared_lock<std::shared_timed_mutex> lock(mActivePublisherMutex);
		js::json pubList = js::json::array();
		for (auto& activeIter : mActivePublishers) {
			js::json pub;
			pub["Publisher_id"] = boost::uuids::to_string(activeIter.first);
			pub["Publisher_name"] = activeIter.second->GetName();
			send info too?? 
			pubList.push_back(pub);
		}
		wm::Message message;
		message.GetJsonMessage() = {
			{"Type", "Publist"},
			{"Publishers", pubList}
		};
		deliver(message);
	}

	void SetPublisher(client_ptr pub) {
		mPublisher = pub;
	}

	virtual void read() {
		do_read();
	}
	virtual void send() {
		do nothing, send is used by the publisher to send to subs
	}

	void WriteBacklog() {
		for (const auto& message : mMessageBacklog) {
			mPublisher->deliver(message);
		}
	}

	void HandleError(const asio::error_code& ec) {
		if (ec.value() == asio::error::shut_down) {
			spdlog::info("{} is shutdown, connection dropped", boost::uuids::to_string(GetClientID()));
		}
		spdlog::error("Subsciber {} I/O error {}, {:d}", boost::uuids::to_string(GetClientID()), ec.message(), ec.value());
	}

	virtual const js::json& get_info() const override {
		return mSubsciberInfo;
	}

private:
	void do_write() {
		auto self = this->shared_from_this();
		asio::async_write(*mSocket, asio::buffer(mMessageStrings.front().data(), mMessageStrings.front().length()), [this, self](const asio::error_code& ec, size_t bytes) {
			OnWrite(ec, bytes);
			});
	}

	void do_read() {
		read from the client
		auto self = std::enable_shared_from_this<Subscriber<socket_type>>::shared_from_this();
		asio::async_read(*mSocket, mReadMessage.GetStreamBuffer(), [this, self](const asio::error_code& ec, size_t bytes) {
			OnRead(ec, bytes);
			});

	}

	void OnWrite(const asio::error_code& ec, size_t bytes) {
		if (ec) {
			error in writing 
			spdlog::error("{}: {}:{:d}", boost::uuids::to_string(GetClientID()), ec.message(), ec.value());
		}
		else {
			mMessageStrings.pop_front();
			if (!mMessageStrings.empty()) {
				do_write();
			}
		}
	}

	void OnRead(const asio::error_code& ec, size_t bytes) {
		if (!ec) {
			if (mReadMessage.ParseStreamBuffer()) {
				if (!mInitialised) {
					InitSubscriber();
				}
				else {
					auto& json = mReadMessage.GetJsonMessage();
					auto iter = mMessageHandlers.find(json["Type"]);
					if (iter != mMessageHandlers.end()) {
						(iter->second(mReadMessage));
					}
					else {
						spdlog::error("{} subscriber: Invalid Message type: {}", boost::uuids::to_string(GetClientID()),
							json["Type"]);
					}
				}
			}
			schedule aother read operation
			do_read();
		}
		else {
			HandleError(ec);
		}
	}

	Message handlers
private:
	void PublisherMesssageHandler(const wm::Message& message) {
		if (mPublisher) {
			mPublisher->deliver(message);
		}
		else {
			mMessageBacklog.push_back(message);
		}
	}

private:
	bool mInitialised;
	js::json mSubsciberInfo;
	wm::Message mReadMessage;
	client_ptr mPublisher;
	std::shared_ptr<socket_t> mSocket;
	std::unordered_map<std::string, std::function<void(const wm::Message&)>> mMessageHandlers;
	std::vector<wm::Message> mMessageBacklog;
	std::deque<std::string> mMessageStrings;
};
*/






/*
	template<typename socket_type>
	class Publisher : public Client, public std::enable_shared_from_this<Publisher<socket_type>>
	{
	public:
		typedef socket_type socket_t;
		constexpr static int MAX_MESSAGES = 100;
		virtual ~Publisher() {
			mActivePublishers.erase(GetClientID());
		}
		Publisher(std::shared_ptr<socket_type> socket) : mSocket(socket), mInitialised(false) {
			//read the data from the client
			read();
		}
		void InitPublisher() {
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
							std::unique_lock<std::shared_timed_mutex> lock(mActivePublisherMutex);
							auto self = this->shared_from_this();
							auto [iter, inserted] = mActivePublishers.insert({ id, self});
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
						}
						else {
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
					SetName(json["Name"]);
					ProcessPendingSubscribers();
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
				}
				else {
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
			}catch (js::json::type_error& error) {
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
			mInitialised = true;
		}
		virtual void deliver(const wm::Message & message) override {
			//deliver message to the clients that are connected as publisher,
			bool write_in_progress = !mMessageStrings.empty();
			auto data = message.GetJsonAsString();
			data += "\r\n";
			mMessageStrings.push_back(data);
			if (!write_in_progress) {
				do_write();
			}
		}
		virtual void read() override {
			//read a message from the client, 
			do_read();
		}

		virtual void send() override {
			//send messages to the subcriber that wants the message
			//only forward messages types that are subsribers messages
			std::shared_lock<std::shared_timed_mutex>(mMutex);
			for (auto& message : mMessageQueue) {
				auto& json = message.GetJsonMessage();
				if (json["Type"] == "Submesg") {
					std::string subid = json["Sub_id"];
					boost::uuids::uuid id = boost::uuids::string_generator()(subid);
					auto iter = mSubscibers.find(id);
					if (iter != mSubscibers.end()) {
						(*iter).second->deliver(message);
					}
				}
			}
		}
		void AddSubscriber(client_ptr subsriber) {
			auto [iter, inserted] = mSubscibers.insert({ subsriber->GetClientID(), std::dynamic_pointer_cast<Subscriber<socket_t>, Client>(subsriber)});
			if (inserted) {
				subsriber->deliver(ComposeWelcomeMessage());
			}
		}
		virtual const js::json& get_info() const override {
			return mPublisherInformation;
		}
	private:

		void do_write() {
			auto self = this->shared_from_this();
			asio::async_write(*mSocket, asio::buffer(mMessageStrings.front().data(), mMessageStrings.front().length()),
				[this, self](const asio::error_code& ec, size_t bytes){
				OnWrite(ec, bytes);
			});
		}

		void do_read() {
			//read a message from the publisher client,
			//schedle a read on the socket
			auto self = this->shared_from_this();
			asio::async_read_until(*mSocket, mReadMessage.GetStreamBuffer(), "\r\n", [this, self](const asio::error_code& ec, size_t bytes){
				OnRead(ec, bytes);
			});
		}

		void ProcessMessage() {
			//some messages are control messages and are not 
		}

		bool verifyJsonObject(js::json & json) {
			//verify that this json object is in the schemea that is required

			return true;
		}

		void ClosePublisher() {


		}

	
		wm::Message ComposeWelcomeMessage() {
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

		void ProcessPendingSubscribers()
		{
			std::shared_lock<std::shared_timed_mutex> lock(mPendingSubscribersMutex);
			auto iter = mPendingSubscribers.find(GetClientID());
			if (iter != mPendingSubscribers.end()) {
				auto& vector = iter->second;
				for (auto sub : vector) {
					auto [iter, inserted] = mSubscibers.insert({ sub->GetClientID(),
						std::dynamic_pointer_cast<Subscriber<socket_t>, Client>(sub)});
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

	private:
		void OnRead(const asio::error_code & ec, size_t bytes_read) {
			if (!ec) {
				//check message 
				//log message header
				if (mReadMessage.ParseStreamBuffer()) {
					if (!mInitialised) {
						InitPublisher();
					}
					else {
						std::unique_lock<std::shared_timed_mutex>(mMutex);
						mMessageQueue.push_back(mReadMessage);
						while (mMessageQueue.size() > MAX_MESSAGES)
							mMessageQueue.pop_front();
					}
				}
				this->do_read();
			}
			else if (ec.value() == asio::error::shut_down) {
				//connetion shut down by the client that is connected as a publisher
				//initiate a close down of the socket and the inform the subscribers that 
				//the publisher if offline... 

			}

			else {
				//error log error
			}
		}

		void OnWrite(const asio::error_code& ec, size_t bytes) {
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

				spdlog::error("Could not write to publisher, publisher shut down {}:{:d}, {}", mSocket->remote_endpoint().address().to_string(),
					mSocket->remote_endpoint().port(), boost::uuids::to_string(GetClientID()));
				//mSocket->close();
			}
			else {
				spdlog::error("Could not write to publisher {}:{:d}, {}", mSocket->remote_endpoint().address().to_string(),
					mSocket->remote_endpoint().port(), boost::uuids::to_string(GetClientID()));
			}
		}


	private:
		bool mInitialised;
		js::json mPublisherInformation;
		wm::Message mReadMessage;
		std::shared_timed_mutex mMutex;
		std::deque<wm::Message> mMessageQueue;
		std::map<boost::uuids::uuid, std::shared_ptr<Subscriber<socket_t>>> mSubscibers;
		std::shared_ptr<socket_t> mSocket;
		std::deque<std::string> mMessageStrings;
	};
*/