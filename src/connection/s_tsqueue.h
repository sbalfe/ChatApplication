#pragma once

#include "netinclude.h"
#include "s_packet.h"


namespace shriller {
	namespace netv2 {

		class safequeue {

		public:
			const MessageVariant& last() {
		
				std::scoped_lock l{ muxQueue };
				
				return que.back();
			}

			const MessageVariant& first() {
				std::scoped_lock l{ muxQueue };
				return que.front();
			}

			size_t size() {
				std::scoped_lock l{ muxQueue };
				return que.size();
			}

			void wipe() {
				std::scoped_lock l{ muxQueue };
				que.clear();
			}

			void addBack(const MessageVariant& newItem) {
			
				std::scoped_lock l{ muxQueue };
				que.emplace_back(newItem);
			}

			void addFront(const MessageVariant& newItem) {
				std::scoped_lock l{ muxQueue };
				que.emplace_front(newItem);
			}

			MessageVariant fetchBack() {
			
				std::scoped_lock l{ muxQueue };
				auto t = std::move(que.back());
				que.pop_back();
				return t;
			}

			MessageVariant fetchFront() {
			
				std::scoped_lock l{ muxQueue };
				auto t = std::move(que.front());
				que.pop_front();
				return t;
			}

			bool isEmpty() {
				std::scoped_lock l{ muxQueue };
				return que.empty();
			}

		private:
			std::deque<MessageVariant> que;
			std::mutex muxQueue;
			std::condition_variable c;

		};
	}
}