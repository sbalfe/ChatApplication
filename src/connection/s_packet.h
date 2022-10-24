#include "netinclude.h"

namespace shriller
{
	namespace netv2
	{

		enum class MessageType {
			BROADCAST_MESSAGE,
			DIRECT_MESSAGE,
			DISCONNECT,
			FIRST_CONNECT
		};

		struct ChatState {
			
			std::vector<std::tuple<std::string, std::string, std::string>> messageHistory;
			//std::map<std::string, std::vector<std::string>> messageHistory;
			std::map<uint16_t, std::string> onlineUsers;
			size_t x{0};

			template<typename Archive>
			void serialize(Archive& ar, const unsigned int version) {
				ar& messageHistory;
				ar& onlineUsers;
				ar& x;
			}
		};

		struct endpoint {
			uint16_t port;
			std::string host;

			template<typename Archive>
			void serialize(Archive& ar, const unsigned int version)
			{
				ar& port;
				ar& host;
				
			}
		};

		struct message
		{

	
			std::string content;
			MessageType type;
			std::string username { };
			std::map < std::string, std::string > mapTest;
		
			template<typename Archive>
			void serialize(Archive& ar, const unsigned int version)
			{
				ar& type;
				ar& content;
				ar& username;
				
			}
		};

		struct ownedMessage {
			message msg;
			uint16_t port;
			template<typename Archive>
			void serialize(Archive& ar, const unsigned int version)
			{
				ar& msg;
				ar& port;
			}

		};

		using MessageVariant = std::variant<std::monostate, ChatState, message, ownedMessage>;

		
	}
}
