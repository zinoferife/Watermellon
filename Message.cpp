#include "Message.h"
using namespace wm;

bool wm::Message::ParseStreamBuffer()
{
	std::stringstream ss;
	ss << &mStreamBuffer;
	auto data = ss.str();
	if (data.empty()){
		return false;
	}
	//remove delimeter
	auto pos = data.find_first_of("\r\n");
	if (pos == std::string::npos) {
		return false;
	}
	auto json_string = data.substr(0, pos);
	mJsonMessage = js::json(json_string);
}

const std::string wm::Message::GetJsonAsString() const
{
	std::stringstream ss;
	ss << mJsonMessage;
	return ss.str();
}

void wm::Message::Clear()
{
	mJsonMessage = {};
}

Message& wm::Message::operator=(const Message& message)
{
	// TODO: insert return statement here
	this->mJsonMessage = message.mJsonMessage;
	return (*this);
}

wm::Message::Message(const Message&& message) noexcept
{
	this->mJsonMessage = std::move(message.mJsonMessage);
}

wm::Message::Message(const Message& message)
{
	this->mJsonMessage = message.mJsonMessage;
}
