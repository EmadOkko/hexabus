#ifndef LIBHEXABUS_SOCKET_HPP
#define LIBHEXABUS_SOCKET_HPP 1

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <string>
#include "../../../shared/hexabus_packet.h"
#include <boost/signals2.hpp>
#include "error.hpp"
#include "packet.hpp"
#include "filtering.hpp"

namespace hexabus {
  class Socket {
    public:
			typedef boost::signals2::signal<
				void (const Packet& packet, const boost::asio::ip::udp::endpoint& from)>
				on_packet_received_t;
			typedef on_packet_received_t::slot_type on_packet_received_slot_t;

			typedef boost::function<bool (const Packet& packet, const boost::asio::ip::udp::endpoint& from)> filter_t;

			typedef boost::signals2::signal<void (const GenericException& error)> on_async_error_t;
			typedef on_async_error_t::slot_type on_async_error_slot_t;

    public:
			static const boost::asio::ip::address_v6 GroupAddress;

      Socket(boost::asio::io_service& io);
      Socket(boost::asio::io_service& io, const std::string& interface);
      ~Socket();

			boost::signals2::connection onPacketReceived(const on_packet_received_slot_t& callback, const filter_t& filter = filtering::any());
			boost::signals2::connection onAsyncError(const on_async_error_slot_t& callback);
      
			void listen(const boost::asio::ip::address_v6& addr);
			void bind(const boost::asio::ip::udp::endpoint& ep);
			void bind(const boost::asio::ip::address_v6& addr) { bind(boost::asio::ip::udp::endpoint(addr, 0)); }

			boost::asio::ip::udp::endpoint localEndpoint() const { return socket.local_endpoint(); }

			void send(const Packet& packet, const boost::asio::ip::udp::endpoint& dest);
			std::pair<Packet::Ptr, boost::asio::ip::udp::endpoint> receive(const filter_t& filter = filtering::any());

			void send(const Packet& packet) { send(packet, GroupAddress); }
			void send(const Packet& packet, const boost::asio::ip::address_v6& dest) { send(packet, boost::asio::ip::udp::endpoint(dest, HXB_PORT)); }

			boost::asio::io_service& ioService() { return io_service; }
    private:
      boost::asio::io_service& io_service;
      boost::asio::ip::udp::socket socket;
			boost::asio::ip::udp::endpoint remoteEndpoint;
			on_packet_received_t packetReceived;
			on_async_error_t asyncError;
			std::vector<char> data;

      void openSocket(const boost::asio::ip::address_v6& addr, const std::string* interface);

			void beginReceive();
			void packetReceiveHandler(const boost::system::error_code& error, size_t size);
  };
};

#endif
