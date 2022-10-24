#include <iostream>
#include <thread>
#include <fstream>
#include <ranges>
#include <variant>
#include <string>

#include <boost/asio.hpp>

#include <s_connection.h>
#include <s_tsqueue.h>

struct ServerState {

    /* username : message : ignore */
    std::vector<std::tuple<std::string, std::string, std::string>> messageHistory;
    std::map <uint16_t, std::tuple<std::string, std::unique_ptr<shriller::netv2::connection>>> onlineUsers_;
    std::mutex readMutex;

};

class Server {
public:
    Server(const uint16_t port) : m_asioAcceptor{ context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port) }, port_{port}{}
    ~Server() {m_contextThread.join();}

    void StartServer() {
        AcceptConnections();
        m_contextThread = std::thread([this]() {context_.run(); });
    }

    bool AcceptConnections() {

        std::cout << "listening for connections " << std::endl;

        m_asioAcceptor.async_accept(
                [this](const std::error_code ec, boost::asio::ip::tcp::socket socket) {


                    uint16_t localPort = socket.remote_endpoint().port();

                    std::cout << "accepted connection: " << localPort<< std::endl;

                    ss_.onlineUsers_.insert({
                                                    localPort,
                                                    std::make_tuple("anonymous", std::make_unique<shriller::netv2::connection>
                                                            (
                                                                    shriller::netv2::owner::server,
                                                                    std::move(socket),
                                                                    context_,
                                                                    msgin_
                                                            ))
                                            });

                    std::get<1>(ss_.onlineUsers_.find(localPort)->second)->connectToClient();
                    AcceptConnections();
                }
        );

        return true;
    }

    auto parseIncoming(const shriller::netv2::MessageVariant&& request){

        auto owner = std::get<3>(request).port;

        return std::make_tuple(std::get<3>(request).msg.content,
                               std::get<3>(request).msg.type,
                               owner,
                               getUsername(owner));
    }

    bool read() {
        if (!msgin_.isEmpty()) {

            auto [content, type, owner, username] = parseIncoming(msgin_.fetchFront());

            switch (type) {

                /* send content to every user in the chatroom */
                case shriller::netv2::MessageType::BROADCAST_MESSAGE:

                    std::cout << "inserting into message history" << std::endl;

                    ss_.messageHistory.emplace_back(std::tuple(username, content, ""));

                    for (auto& [port, conn] : ss_.onlineUsers_) {
                        std::get<1>(conn)->send(shriller::netv2::message{ content , shriller::netv2::MessageType::DIRECT_MESSAGE, username });
                    }
                    break;

                    /* disconnect by choice but may be forceful disconnection */
                case shriller::netv2::MessageType::DISCONNECT: {
                    std::cout << "disconnect request" << std::endl;
                    ss_.onlineUsers_.erase(owner);
                    std::stringstream ss;
                    //ss << std::format("{} has left the server", username);
                    ss_.messageHistory.emplace_back(std::tuple("server", ss.str(), username));
                    broadcast(ss.str(), username);
                    break;

                    /* sent on first connection to establish username*/
                }
                case shriller::netv2::MessageType::FIRST_CONNECT: {
                    getUsername(owner) = content;
                    std::stringstream ss;
                    //ss << std::format("{} has joined the server", content);
                    ss_.messageHistory.emplace_back(std::tuple("server", ss.str(), content));
                    broadcast(ss.str(), content);
                }
                default: ;
            }
        }
        return true;
    }

    void broadcast(const std::string& message, const std::string_view ignore) {
        for (auto& [client, conn] : ss_.onlineUsers_) {

            if (getUsername(client) != ignore) {

                std::get<1>(conn)->send(
                        shriller::netv2::message{
                                message,
                                shriller::netv2::MessageType::DIRECT_MESSAGE, "server" }
                );
            }
        }
    }

    void sendState() {

        /* prevents the queue being overloaded with an endless number of state updates */
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        std::map<uint16_t, std::string> onlineUsers_s;

        for (auto& [client, conn] : ss_.onlineUsers_) {
            onlineUsers_s.insert({ client, std::get<0>(conn) });
        }

        for (auto& [port , conn] : ss_.onlineUsers_) {

            if (ss_.messageHistory.empty()) continue;


            std::get<1>(conn)->send(shriller::netv2::ChatState{ ss_.messageHistory , onlineUsers_s });
        }
    }

    void errorChecker() {

        for (auto it = ss_.onlineUsers_.begin(); it != ss_.onlineUsers_.end();)
        {

            const auto error = std::get<1>(it->second)->readError();
            auto username = getUsername(it->first);

            switch (error) {
                case 10054: {

                    /* remove the user*/
                    ss_.onlineUsers_.erase(it++);

                    /* construct a message to send to the brodcast */
                    std::stringstream ss;
                    //ss << std::format("{} has left the server", username);
                    ss_.messageHistory.emplace_back(std::tuple("server", ss.str(), username));
                    broadcast(ss.str(), username);
                    break;
                }
                default:
                    ++it;
            }


        }
    }

private:

    std::string& getUsername(const uint16_t& port) {
        return std::get<0>(ss_.onlineUsers_.find(port)->second);
    }

    boost::asio::io_context context_;
    std::thread m_contextThread;
    boost::asio::ip::tcp::acceptor m_asioAcceptor;
    ServerState ss_;
    std::map<uint16_t, std::unique_ptr<shriller::netv2::connection>> onlineUsers_;
    shriller::netv2::safequeue msgin_;
    uint16_t port_;

};

int main()
{
    Server server{ 60000 };

    server.StartServer();
    while (true) {


        std::jthread readThread{ [&]() {
            while (true) {
                server.read();

            }
        } };

        std::jthread stateThread{ [&]() {
            while (true) {
                server.sendState();
            }
        } };


        std::jthread errorThread([&]() {
            while (true) {
                server.errorChecker();
            }
        });


    }



}

