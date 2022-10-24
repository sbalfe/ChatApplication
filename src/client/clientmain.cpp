
#include "clientmain.h"


Client::Client() {}
Client::~Client() {
	m_thread.join();

}

void Client::ConnectToServer(const std::string& host, const std::uint16_t& port) {


	connection_ = std::make_unique<shriller::netv2::connection>(shriller::netv2::owner::client, boost::asio::ip::tcp::socket{ m_context }, m_context, msgin_);

	boost::asio::ip::tcp::resolver resolver { m_context };
	boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

	
	connection_->connectToServer(endpoints);
	

	m_thread = std::thread([this]() { m_context.run(); });

}

void Client::send(shriller::netv2::message mes) const {
	connection_->send(mes);	
}

[[maybe_unused]] bool Client::connected() {
	return connection_->isOpen();
}

std::optional<shriller::netv2::MessageVariant> Client::read() {

	std::optional<shriller::netv2::MessageVariant> retopt;

	if (!msgin_.isEmpty()) {
		return msgin_.fetchFront();
	}

	return retopt.value_or(std::monostate{});
}

