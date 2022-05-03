#pragma once
#include <iostream>
#include <spdlog/spdlog.h>
#include "Client.h"
#include <deque>
#include <memory>


//listens for messages that the publisher publishes
//Query the server for information
//ask the publisher for data aswell

namespace wm {
	
	class Subscriber : public Client, public std::enable_shared_from_this<Subscriber>
	{
	public:
		Subscriber(std::shared_ptr<asio::ip::tcp::socket> socket);
		virtual ~Subscriber() {}
		virtual void deliver(const wm::Message& message) override;

		void InitSubscriber();
		void RemovePublisher();
		//for now only send the list of active publishers, 
		//later would have to find a way to read and send all publishers
		void SendListOfActivePublishers();

		inline void SetPublisher(client_ptr pub) { mPublisher = pub; }
		virtual void read() override;
		virtual void send() override;
		virtual void Start() override;
		virtual void Close() override;
		virtual void ShutDown() override;

		void WriteBacklog();
		void DeleteSubscriber();
		void HandleError(const asio::error_code& ec);
		void InformPublisher();


		virtual const js::json& get_info() const override {
			return mSubsciberInfo;
		}

	private:
		void do_write();
		void do_read();
		void OnWrite(const asio::error_code& ec, size_t bytes);
		void OnRead(const asio::error_code& ec, size_t bytes);

	//Message handlers
	public:
		static void InitMessageHandlers();
		void PublisherMesssageHandler(const wm::Message& message);
		void ServerMessageHandler(const wm::Message& message);

	private:
		bool mInitialised;
		js::json mSubsciberInfo;
		wm::Message mReadMessage;
		client_ptr mPublisher;
		std::shared_ptr<asio::ip::tcp::socket> mSocket;
		std::vector<wm::Message> mMessageBacklog;
		std::deque<std::string> mMessageStrings;
	};

}
