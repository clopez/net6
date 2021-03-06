/* net6 - Library providing IPv4/IPv6 network access
 * Copyright (C) 2005 Armin Burgmeier / 0x539 dev group
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.hpp"

#include "error.hpp"
#include "socket.hpp"

#ifdef WIN32
# define WIN32_CAST_FIX(a) (static_cast<char*>(a) )
# define WIN32_CCAST_FIX(a) (static_cast<const char*>(a) )
#else
# define WIN32_CAST_FIX(a) (a)
# define WIN32_CCAST_FIX(a) (a)
# include <unistd.h>
#endif

namespace
{
	int address_to_protocol(int af)
	{
		switch(af)
		{
#ifndef WIN32
		case AF_UNIX: return PF_UNIX;
#endif
		case AF_INET: return PF_INET;
		case AF_INET6: return PF_INET6;
		default:
			throw net6::error(
				net6::error::ADDRESS_FAMILY_NOT_SUPPORTED
			);
		}
	}

	void set_nosigpipe(net6::tcp_socket::socket_type socket)
	{
#if HAVE_SO_NOSIGPIPE
		// Mac OS X
		int value = 1;
		if(setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &value,
				sizeof(int)) == -1)
			throw net6::error(net6::error::SYSTEM);
#endif
	}

	void set_reuseaddr(net6::tcp_socket::socket_type socket)
	{
#ifndef WIN32
		// Allow fast restarts of servers by enabling SO_REUSEADDR
		int value = 1;
		if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &value,
				sizeof(int)) == -1)
			throw net6::error(net6::error::SYSTEM);
#endif
	}

#ifndef WIN32
	const int INVALID_SOCKET = -1;
#endif
}

net6::socket::socket(int domain, int type, int protocol):
	sock(::socket(domain, type, protocol) )
{
	if(sock == INVALID_SOCKET)
		throw error(error::SYSTEM);
}

net6::socket::socket(socket_type c_object):
	sock(c_object)
{
}

net6::socket::~socket()
{
	if(sock != INVALID_SOCKET)
	{
#ifdef WIN32
		closesocket(cobj() );
#else
		close(cobj() );
#endif
	}
}

void net6::socket::invalidate()
{
	sock = INVALID_SOCKET;
}

net6::tcp_socket::tcp_socket(const address& addr):
	socket(address_to_protocol(addr.get_family()), SOCK_STREAM, 0)
{
}

net6::tcp_socket::tcp_socket(socket_type c_object):
	socket(c_object)
{
}

net6::tcp_client_socket::tcp_client_socket(const address& addr):
	tcp_socket(addr)
{
	if(::connect(cobj(), addr.cobj(), addr.get_size()) == -1)
		throw error(net6::error::SYSTEM);

	set_nosigpipe(cobj() );
}

net6::tcp_client_socket::tcp_client_socket(socket_type c_object):
	tcp_socket(c_object)
{
	set_nosigpipe(c_object);
}

net6::tcp_client_socket::~tcp_client_socket()
{
}

net6::socket::size_type net6::tcp_client_socket::send(const void* buf,
                                                      size_type len) const
{
	ssize_t result = ::send(
		cobj(),
		WIN32_CCAST_FIX(buf),
		len,
#ifdef HAVE_MSG_NOSIGNAL
		MSG_NOSIGNAL
#else
		0
#endif
	);

	if(result < 0)
		throw error(net6::error::SYSTEM);

	return result;
}

net6::socket::size_type net6::tcp_client_socket::recv(void* buf,
                                                      size_type len) const
{
	ssize_t result = ::recv(
		cobj(),
		WIN32_CAST_FIX(buf),
		len,
#ifdef HAVE_MSG_NOSIGNAL
		MSG_NOSIGNAL
#else
		0
#endif
	);

	if(result < 0)
		throw error(net6::error::SYSTEM);

	return result;
}

net6::tcp_server_socket::tcp_server_socket(const address& bind_addr):
	tcp_socket(bind_addr)
{
	set_reuseaddr(cobj() );
	if(bind(cobj(), bind_addr.cobj(), bind_addr.get_size()) == -1)
		throw error(net6::error::SYSTEM);
	if(listen(cobj(), 0) == -1)
		throw error(net6::error::SYSTEM);
}

net6::tcp_server_socket::tcp_server_socket(socket_type c_object):
	tcp_socket(c_object)
{
}

std::auto_ptr<net6::tcp_client_socket> net6::tcp_server_socket::accept() const
{
	socket_type new_sock = ::accept(cobj(), NULL, NULL);
	if(new_sock == INVALID_SOCKET)
		throw error(net6::error::SYSTEM);

	return std::auto_ptr<tcp_client_socket>(
		new tcp_client_socket(new_sock)
	);
}

std::auto_ptr<net6::tcp_client_socket>
net6::tcp_server_socket::accept(address& from) const
{
	socklen_t sock_size = from.get_size();
	socket_type new_sock = ::accept(cobj(), from.cobj(), &sock_size);
	if(new_sock == INVALID_SOCKET)
		throw error(net6::error::SYSTEM);

	return std::auto_ptr<tcp_client_socket>(
		new tcp_client_socket(new_sock)
	);
}

net6::udp_socket::udp_socket(const address& bind_addr):
	socket(
		address_to_protocol(bind_addr.get_family()),
		SOCK_DGRAM,
		IPPROTO_UDP
	)
{
	if(::bind(cobj(), bind_addr.cobj(), bind_addr.get_size()) == -1)
		throw error(net6::error::SYSTEM);
}

void net6::udp_socket::set_target(const address& addr)
{
	if(connect(cobj(), addr.cobj(), addr.get_size()) == -1)
		throw error(net6::error::SYSTEM);
}

void net6::udp_socket::reset_target()
{
	if(connect(cobj(), NULL, 0) == -1)
		throw error(net6::error::SYSTEM);
}

net6::socket::size_type
net6::udp_socket::send(const void* buf, size_type len) const
{
	ssize_t result = ::send(cobj(), WIN32_CCAST_FIX(buf), len, 0);
	if(result == -1)
		throw error(net6::error::SYSTEM);

	return result;
}

net6::socket::size_type
net6::udp_socket::send(const void* buf, size_type len, const address& to) const
{
	ssize_t result = ::sendto(cobj(), WIN32_CCAST_FIX(buf), len, 0,
	                          to.cobj(), to.get_size());
	if(result == -1)
		throw error(net6::error::SYSTEM);

	return result;
}

net6::socket::size_type
net6::udp_socket::recv(void* buf, size_type len) const
{
	ssize_t result = ::recv(cobj(), WIN32_CAST_FIX(buf), len, 0);
	if(result == -1)
		throw error(net6::error::SYSTEM);

	return result;
}

net6::socket::size_type
net6::udp_socket::recv(void* buf, size_type len, address& from) const
{
	socklen_t sock_size = from.get_size();
	ssize_t result = ::recvfrom(cobj(), WIN32_CAST_FIX(buf), len, 0,
	                            from.cobj(), &sock_size);
	if(result == -1)
		throw error(net6::error::SYSTEM);

	return result;
}
