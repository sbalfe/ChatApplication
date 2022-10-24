
#include "netinclude.h"
#include "s_tsqueue.h"



namespace shriller {
	namespace netv2 {

		
		constexpr size_t HEADER_LENGTH = 64;
			
		enum class owner {
			server,
			client
		};

		enum MessageTypes {
			empty,
			chatState,
			normalMessage,
			messageOwned
		};

		enum QueueTypes {
			state_,
			message_,
		};

		struct info {
			std::size_t inboundDataSize;
			int index;
		};

		using MessageTypeVar = std::variant <std::monostate,  message, ChatState> ;


		class connection : public std::enable_shared_from_this<connection> {
		
		public:

			connection(owner ownert, boost::asio::ip::tcp::socket socket, boost::asio::io_context& ctx, safequeue& msgin) :
				socket_{ std::move(socket) }, ctx_{ ctx }, msgin_{ msgin } {
				ownertype_ = ownert;
			}

			template<typename ... lambdas>
			struct overload : lambdas... {
				using lambdas::operator()...;
			};

		
			void serializeOutbound() {
			
				MessageVariant msg = msgout_.first();
				
				std::ostringstream arcstrm;
				boost::archive::text_oarchive arc(arcstrm);
				
				overload visitor{[&]<typename type>(type data) {
					if constexpr (!std::is_same_v<type, std::monostate>) {
						arc << data;
					}
				}};

				std::visit(visitor, msg);

				outboundbody = arcstrm.str();

			}
			void HandleError(const std::error_code& ec) {


				switch (ec.value()) {
				case 10061 :
					std::cout << "failure to connect to host" << std::endl;
					//socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
					socket_.close();
					break;
				case 10054:
					std::cout << "client has left" << std::endl;
					errorCode = 10054;
					break;
				default :
					std::cout << "error in handle error: " << ec.message() << std::endl;
				}
			}

			void connectToServer(const boost::asio::ip::tcp::resolver::results_type& eps) {
				boost::asio::async_connect(socket_, eps,
						[this](const std::error_code ec, boost::asio::ip::tcp::endpoint [[maybe_unused]] endpoint)
						{
							if (!ec)
							{ 
								ReadHeader();
							}
							else {
								
								HandleError(ec);
							}
				});
			}

			bool connectToClient() {
				if (ownertype_ == owner::server)
				{
					if (socket_.is_open()) {
						ReadHeader();
						return true;
					}
					return false;
				}

				return false;
			}


			bool isOpen() const {
				
				return socket_.is_open();
			}
		
	
			void send(const MessageVariant&& msg) {
				
				boost::asio::post(ctx_, [this, msg]() {

					const bool hasAnItem = !msgout_.isEmpty();

					msgout_.addBack(msg);

					if (!hasAnItem) {
						writeIndex_ = static_cast<int>(msg.index());
						WriteHeader();
					} 
				});
				
			}

			void ReadHeader() {
			
				boost::asio::async_read(socket_, boost::asio::buffer(tempHeaderInbound_, HEADER_LENGTH),
					[this](const std::error_code ec, std::size_t bytes){
							if (!ec){

								/* init info struct to read incoming header information */
								info i {0, 0};
								
								std::istringstream is{ std::string{tempHeaderInbound_, HEADER_LENGTH} };
								std::string temp;
							
								size_t counter{};

								/* string read in */
								while (std::getline(is, temp, ',')) {
									std::erase_if(temp, isspace);
									
									switch (counter) {

									case 0: {
										std::stringstream stream{ temp };
										stream >> std::hex >> i.inboundDataSize;
										break;
									}
									case 1:
										i.index = std::stoi(temp);
										break;
									default: break;
									}
									++counter;
								}
								tempBodyInbound_.resize(i.inboundDataSize);
								readIndex_ = i.index;
							    
								ReadBody();
							}
					}
				);
			}

			void ReadBody() {

				boost::asio::async_read(socket_, boost::asio::buffer(tempBodyInbound_.data(), tempBodyInbound_.size()),
					[this](const std::error_code ec , std::size_t bytes){
						
						
						std::string arcdata{tempBodyInbound_.data(), tempBodyInbound_.size()};
					
						std::istringstream arcstrm{ arcdata };
						boost::archive::text_iarchive arc { arcstrm };

						switch (readIndex_) {
						case 0:
							tmpmsgin_ = std::monostate{};
							break;
						case 1: {
							shriller::netv2::ChatState cs{};
							arc >> cs;
							tmpmsgin_ = cs;
							break;
						}
						case 2: {
							shriller::netv2::message mes{};
							arc >> mes;
							tmpmsgin_ = mes;
							break;
						}
						default: break;
						}

						addToQueue();
						 
					}
				);
			}


			void WriteHeader() {
				serializeOutbound();

				std::ostringstream header;

				/* header: set number of spaces to be the size of header
				 *
				 * encode the size of the body and type of message being sent denoted by the writeIndex_
				 */
				//header << std::setw(HEADER_LENGTH) << std::hex << std::format("{:x},{}", outboundbody.size(), writeIndex_);
				
				headerOutbound_ = header.str();
				
				async_write(socket_, boost::asio::buffer(headerOutbound_),
					[this](const std::error_code ec, std::size_t bytes) {
						if (!ec) {						
							WriteBody();
						}
						else {
							HandleError(ec);
						}
					}
				);
				
			}

			void WriteBody() {
				async_write(socket_, boost::asio::buffer(outboundbody),
					[this](const std::error_code ec, std::size_t bytes) {
						msgout_.fetchFront();
						
						if (!msgout_.isEmpty()) {
							WriteHeader();
							
						}
					}
				
				);
			}

			void addToQueue() {

				MessageVariant m_var;
				
				overload visitor{ [&]<typename type>(type data) {
				
					if constexpr (std::is_same_v<type, message>) {
						if (ownertype_ == owner::server) {
							m_var.emplace<messageOwned>(std::get<message_>(tmpmsgin_), socket_.remote_endpoint().port());
							return;
						}
					}
					m_var = data;
				} };

				std::visit(visitor, tmpmsgin_);

				msgin_.addBack(m_var);

				ReadHeader();
			}

			[[nodiscard]] auto remoteEndpoint() const{
				return socket_.remote_endpoint();
			}

			[[nodsicard]] int readError() const{
				return errorCode;
			}
			
		private:
			boost::asio::ip::tcp::socket socket_; 
			boost::asio::io_context& ctx_;
			safequeue msgout_;
			safequeue& msgin_;
			int errorCode{0};

			char tempHeaderInbound_[HEADER_LENGTH];
			std::vector<char> tempBodyInbound_;
			std::string tempHeaderInbound_s;
			MessageTypeVar tmpmsgin_;
			std::string outboundbody;
			std::string outboundHeader;
			size_t outBoundSize{};
			owner ownertype_ = owner::server;
			uint32_t id_ = 0;
			std::string headerOutbound_;
			int writeIndex_ {};
			int readIndex_  {};
		};

	}
}
