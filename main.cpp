#include <iostream>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <asio.hpp>

#include "Server.h"

int main(int argc, char** argv)
{
	try {
		asio::io_context service;
		wm::Server server(service);
		server.StartServer();
		std::string stop;
		std::cin >> stop;
		server.StopServer();
	}
	catch (asio::system_error& error){
		spdlog::error("{} {:d}", error.what(), error.code().value());
	}
	catch (...) {
		spdlog::critical("Uncaught exception!!");
	}
	return 0;
}