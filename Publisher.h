#pragma once
#include "Client.h"
#include <deque>
#include <unordered_map>
#include <functional>
#include <map>
#include <shared_mutex>
#include <sstream>
#include <tuple>

#include "Subscriber.h"

namespace wm {

	//subcribers subscrib to publisers to publish data to them
	//template on the socket type ??? 
	extern std::unordered_map<std::string, std::function<Message(Message& message)>> mMessageHandlers;
	extern void InitHandlers();
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
			do_write(message);
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

		void do_write(const wm::Message & message) {
			auto data = message.GetJsonAsString();
			data += "\r\n";
			auto self = this->shared_from_this();
			asio::async_write(*mSocket, asio::buffer(data), [this, self](const asio::error_code& ec, size_t bytes){
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
				return;
			}
			else if (ec.value() == asio::error::shut_down) {
				//connection closed. initiate a close operation on the publisher 
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
	};
}