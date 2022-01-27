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
		Subscriber(std::shared_ptr<socket_type> socket) : mSocket(socket) {
			SetUpMessageHandlers();
			read();
		}
		virtual ~Subscriber() {}
		virtual void deliver(const wm::Message& message) override {
			//deliver a message to the client that is connected as a subscriber
			do_write(message);
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
										//contine with the init process, store subscriber in the 
										//pending subsciber list, awaiting the publisher to come online
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
								//invalid or null publisher id,
								//send the list of active publishers to the client to choose publisher
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
					//initalised
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

		//for now only send the list of active publishers, 
		//later would have to find a way to read and send all publishers
		void SendListOfActivePublishers() {
			std::shared_lock<std::shared_timed_mutex> lock(mActivePublisherMutex);
			js::json pubList = js::json::array();
			for (auto& activeIter : mActivePublishers) {
				js::json pub;
				pub["Publisher_id"] = boost::uuids::to_string(activeIter.first);
				pub["Publisher_name"] = activeIter.second->GetName();
				//send info too?? 
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
			//do nothing, send is used by the publisher to send to subs
		}

		void WriteBacklog(){
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
		void do_write(const wm::Message& message){
			auto data = message.GetJsonAsString();
			data += "\r\n";
			auto self = this->shared_from_this();
			asio::async_write(*mSocket, asio::buffer(data), [this, self](const asio::error_code& ec, size_t bytes){
				OnWrite(ec, bytes);
			});
		}

		void do_read() {
			//read from the client
			auto self = this->shared_from_this();
			asio::async_read(*mSocket, mReadMessage.GetStreamBuffer(), [this, self](const asio::error_code& ec, size_t bytes){
				OnRead(ec, bytes);
			});

		}

		void OnWrite(const asio::error_code& ec, size_t bytes) {
			if(ec){
				//error in writing 
				spdlog::error("{}: {}:{:d}", boost::uuids::to_string(GetClientID()), ec.message(), ec.value());
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
				//schedule aother read operation
				do_read();
			} else {
				HandleError(ec);
			}
		}

	//Message handlers
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
	};

}
