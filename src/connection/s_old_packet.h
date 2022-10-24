#pragma once

#include "netinclude.h"

namespace shriller {
	
	namespace net {

		enum class MessageType {

			BROADCAST_MESSAGE,
			DIRECT_MESSAGE,
			CONNECT,
			DISCONNECT
		};

		template<typename T> 
		std::vector<char> serialize(T&& object)
		{
		
			std::vector<char> bytes;
			bytes.resize(sizeof object);
			new(bytes.data()) T(std::forward<T>(object));
			return bytes;
		}

		template<typename T> 
		T deserialize(std::vector<char>& bytes)
		{
		
			T* data = std::launder(reinterpret_cast<T*>(bytes.data()));
		
			T object = std::move(*data);
			
			data->~T();
			
			return object;
		}

		struct header {
			shriller::net::MessageType messageID;
			size_t bodySize;
		};

		
		class packet {

		private:

			struct header {
				shriller::net::MessageType messageID;
				size_t bodySize;
			};

			struct body { 

				std::vector<char> payload;
				std::make_pair<char> payload.
			};

		public:

			packet(){}

			template<typename T>
			packet(const MessageType& packetContext, T&& data)
	        {
			

				size_t currentSize = packetBody.payload.size();

				packetBody.payload.resize(packet.payload.size() + currentSize);

				std::memcpy(packetBody.payload.data() + currentData, &data, sizeof(T));
				

				
				//std::vector<char> serializeData = serialize<T>(std::move(data));

				//std::cout << "size of data" << serializeData.size() << std::endl;
				
				
				packetHeader = header{ packetContext, size};

			
				packetBody = body( std::move(data) );
			

			
			
			}

			void setSize(const size_t& size) {
				packetHeader.bodySize = size;
			}
			header packetHeader;
			body packetBody;
		};
	}
}