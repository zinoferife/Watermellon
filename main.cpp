#include <iostream>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <asio.hpp>


int main(int argc, char** argv)
{
	asio::io_context service;
	asio::ip::tcp::socket sock(service);
	asio::ip::tcp::endpoint ed{ asio::ip::address::from_string("127.0.0.1"), 3333 };
	try {
		sock.open(ed.protocol());
		sock.connect(ed);

		std::array<char, 1024> buffer;
		sock.read_some(asio::buffer(std::addressof(buffer), 1024));
	}
	catch (asio::system_error& error){
		spdlog::error("{} {:d}", error.what(), error.code().value());
	}

	spdlog::info("Hello, Watermellon server Log");
	std::cout << "Hello, Watermellon server!" << std::endl;
	return 0;
}