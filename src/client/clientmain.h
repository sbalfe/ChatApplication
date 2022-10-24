#pragma once

#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <thread>
#include <s_tsqueue.h>
#include <s_connection.h>


#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_stdlib.h"
#include <GLFW/glfw3.h>

class Client {
public:
	Client();
	~Client();
	void ConnectToServer(const std::string& host, const std::uint16_t& port);
	void send(shriller::netv2::message) const;
    [[maybe_unused]] bool connected();
	std::optional<shriller::netv2::MessageVariant> read();
private:
	std::unique_ptr<shriller::netv2::connection> connection_;
	std::thread m_thread;
	boost::asio::io_context m_context;
	shriller::netv2::safequeue msgin_;
};
