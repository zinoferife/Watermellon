#pragma once
#include <nlohmann/json.hpp>
#include <asio.hpp>
#include <sstream>
#include <iostream>

namespace js = nlohmann;

namespace wm {
	//all messages passed in is just a json object
	class Message
	{
	public:
		Message() {}
		inline asio::streambuf& GetStreamBuffer() { return mStreamBuffer; }
		inline js::json& GetJsonMessage() { return mJsonMessage; }
		inline const asio::streambuf& GetStreamBuffer() const { return mStreamBuffer; }
		inline const js::json& GetJsonMessage() const  { return mJsonMessage; }
		bool ParseStreamBuffer();
		const std::string GetJsonAsString() const;
		void Clear();
		//value symantics
		Message& operator=(const Message& message);
		Message(const Message&& message) noexcept;
		Message(const Message& message);
	private:
		//read until \r\n 
		asio::streambuf mStreamBuffer;
		js::json mJsonMessage;
	};
}
