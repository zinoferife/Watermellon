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
		bool ParseStreamBuffer();

	private:
		//read until \r\n 
		asio::streambuf mStreamBuffer;
		js::json mJsonMessage;
	};
}
