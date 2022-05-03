#pragma once
#include "Client.h"
#include <deque>
#include <unordered_map>
#include <functional>
#include <map>
#include <shared_mutex>
#include <sstream>
#include <tuple>
#include <deque>


#include "Subscriber.h"

namespace wm {

	class Publisher : public Client, public std::enable_shared_from_this<Publisher>
	{
	public:
		constexpr static int MAX_MESSAGES = 100;
		virtual ~Publisher();
		Publisher(std::shared_ptr<asio::ip::tcp::socket> socket);
		void InitPublisher();
		virtual void deliver(const wm::Message& message) override;
		virtual void read() override;
		virtual void send() override;
		virtual void Start() override;
		virtual void Close() override;
		virtual void ShutDown() override;
		void AddSubscriber(client_ptr subsriber);
		virtual const js::json& get_info() const override {
			return mPublisherInformation;
		}

		static void InitMessageHandlers();
	
	//message handlers
	public:
		void OnSubMessage(const wm::Message& message);
		void OnServerMessage(const wm::Message& message);
	private:

		void do_write();
		void do_read();
		void ProcessMessage();
		bool verifyJsonObject(js::json& json);
		void ClosePublisher();
		wm::Message ComposeWelcomeMessage();
		void ProcessPendingSubscribers();
	private:
		void OnRead(const asio::error_code& ec, size_t bytes_read);
		void OnWrite(const asio::error_code& ec, size_t bytes);

	private:
		bool mInitialised;
		js::json mPublisherInformation;
		wm::Message mReadMessage;
		std::map<boost::uuids::uuid, std::shared_ptr<Subscriber>> mSubscibers;
		std::shared_ptr<asio::ip::tcp::socket> mSocket;
		std::deque<std::string> mMessageStrings;
	};
}