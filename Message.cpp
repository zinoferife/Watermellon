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
