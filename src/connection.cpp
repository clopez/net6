/* net6 - Library providing IPv4/IPv6 network access
 * Copyright (C) 2005, 2006 Armin Burgmeier / 0x539 dev group
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

#include <iostream>

#include "error.hpp"
#include "connection.hpp"

net6::connection_base::connection_base():
	remote_sock(NULL),
	encrypted_sock(NULL),
	remote_addr(NULL),
	state(CLOSED)
{
}

net6::connection_base::~connection_base()
{
}

void net6::connection_base::connect(const address& addr)
{
	if(state != CLOSED)
	{
		throw std::logic_error(
			"net6::connection_base::connect:\n"
			"Connection is not closed"
		);
	}

	remote_sock.reset(new tcp_client_socket(addr) );
	setup_signal();

	remote_addr.reset(addr.clone() );
	state = UNENCRYPTED;

	set_select(IO_ERROR | IO_INCOMING);
}

void net6::connection_base::assign(std::auto_ptr<tcp_client_socket> sock,
                                   const address& addr)
{
	if(state != CLOSED)
	{
		throw std::logic_error(
			"net6::connection_base::assign:\n"
			"Connection is not closed"
		);
	}

	remote_sock = sock;
	setup_signal();

	remote_addr.reset(addr.clone() );
	state = UNENCRYPTED;

	set_select(IO_ERROR | IO_INCOMING);
}

const net6::address& net6::connection_base::get_remote_address() const
{
	return *remote_addr;
}

void net6::connection_base::send(const packet& pack)
{
	if(state == CLOSED)
	{
		throw std::logic_error(
			"net6::connection_base::send:\n"
			"Connection is closed"
		);
	}

	pack.enqueue(sendqueue);

	if(sendqueue.get_size() > 0)
	{
		io_condition flags = get_select();
		if( (flags & IO_OUTGOING) == 0)
			set_select(flags | IO_OUTGOING);
	}
}

void net6::connection_base::request_encryption(bool as_client)
{
	if(state != UNENCRYPTED)
	{
		throw std::logic_error(
			"net6::connection::request_encryption:\n"
			"Encryption request has already been performed"
		);
	}

	// Request encryption from other side
	packet pack("net6_encryption");
	pack << as_client;
	send(pack);

	// Wait for net6_encryption_ok or net6_encryption_failed
	if(as_client)
		state = ENCRYPTION_REQUESTED_CLIENT;
	else
		state = ENCRYPTION_REQUESTED_SERVER;

	// Block further outgoing traffic
	sendqueue.block();
}

net6::connection_base::signal_recv_type
net6::connection_base::recv_event() const
{
	return signal_recv;
}

net6::connection_base::signal_send_type
net6::connection_base::send_event() const
{
	return signal_send;
}

net6::connection_base::signal_close_type
net6::connection_base::close_event() const
{
	return signal_close;
}

net6::connection_base::signal_encrypted_type
net6::connection_base::encrypted_event() const
{
	return signal_encrypted;
}

net6::connection_base::signal_encryption_failed_type
net6::connection_base::encryption_failed_event() const
{
	return signal_encryption_failed;
}

void net6::connection_base::on_sock_event(io_condition io)
{
	try
	{
		do_io(io);
	}
	catch(net6::error& e)
	{
		// Asynchronous send/recv on WIN32 requires that one
		// reads/writes from/to the socket until WSAEWOULDBLOCK
		// occurs. We catch this case here to allow
		// asynchronous selection on WIN32.
#ifdef WIN32
		if(e.get_code() == error::WOULD_BLOCK)
			return;
#endif
		if(e.get_code() == error::CONNECTION_RESET ||
		   e.get_code() == error::BROKEN_PIPE ||
		   e.get_code() == error::PULL_ERROR ||
		   e.get_code() == error::PUSH_ERROR ||
		   e.get_code() == error::UNEXPECTED_PACKET_LENGTH) // TLS...
		{
			on_close();
		}
		else
		{
			throw e;
		}
	}
}

void net6::connection_base::do_io(io_condition io)
{
	if(io & IO_INCOMING)
	{
		if(state == ENCRYPTION_HANDSHAKING)
		{
			do_handshake();
			return;
		}

		char buffer[1024];
		socket::size_type bytes = remote_sock->recv(buffer, 1024);

		if(bytes == 0)
		{
			on_close();
			return;
		}

		recvqueue.append(buffer, bytes);

		// Clear remaining data in GnuTLS cache
		if(encrypted_sock != NULL && encrypted_sock->get_pending() > 0)
		{
			tcp_encrypted_socket_base::size_type size =
				encrypted_sock->get_pending();

			char* ch_buf = new char[size];
			bytes = remote_sock->recv(ch_buf, size);

			recvqueue.append(ch_buf, size);
			delete[] ch_buf;

			if(bytes != size)
			{
				throw std::logic_error(
					"net6::connection::do_io:\n"
					"Did not receive all data from "
					"GnuTLS cache"
				);
			}
		}

		// Store packets first to allow signal handlers to
		// delete the connection object
		std::list<packet> packet_list;

		try
		{
			while(true)
			{
				packet_list.push_back(packet(recvqueue) );
			}
		}
		catch(packet::end_of_queue&) {}

		// Emit signal now as we do not depend anymore on members.
		for(std::list<packet>::iterator iter = packet_list.begin();
		    iter != packet_list.end();
		    ++ iter)
		{
			on_recv(*iter);
		}
	}

	if(io & IO_OUTGOING)
	{
		if(state == ENCRYPTION_HANDSHAKING)
		{
			do_handshake();
			return;
		}

		if(sendqueue.get_size() == 0)
		{
			throw std::logic_error(
				"net6::connection::do_io:\n"
				"Nothing to send in send queue"
			);
		}

		socket::size_type bytes = remote_sock->send(
			sendqueue.get_data(),
			sendqueue.get_size()
		);

		if(bytes <= 0)
		{
			on_close();
			return;
		}

		sendqueue.remove(bytes);
		if(sendqueue.get_size() == 0)
			on_send();
	}

	if(io & IO_ERROR)
	{
		on_close();
	}
}

void net6::connection_base::on_recv(const packet& pack)
{
	try
	{
		do_recv(pack);
	}
	catch(net6::bad_count& e)
	{
		std::cerr << "net6 warning: Protocol mismatch! Received bad "
		          << "parameter count from " << remote_addr->get_name()
		          << " in packet " << pack.get_command() << std::endl;
	}
	catch(net6::bad_format& e)
	{
		std::cerr << "net6 warning: Protocol mismatch! Received bad "
		          << "parameter format from " << remote_addr->get_name()
		          << " in packet " << pack.get_command() << ": "
		          << e.what() << std::endl;
	}
	catch(net6::bad_value& e)
	{
		std::cerr << "net6 warning: Protocol mismatch! Received bad "
		          << "parameter value from " << remote_addr->get_name()
		          << " in packet " << pack.get_command() << ": "
		          << e.what() << std::endl;
	}
}

void net6::connection_base::do_recv(const packet& pack)
{
	if(pack.get_command() == "net6_encryption")
		net_encryption(pack);
	else if(pack.get_command() == "net6_encryption_ok")
		net_encryption_ok(pack);
	else if(pack.get_command() == "net6_encryption_failed")
		net_encryption_failed(pack);
	else if(pack.get_command() == "net6_encryption_begin")
		net_encryption_begin(pack);
	else
		signal_recv.emit(pack);
}

void net6::connection_base::begin_handshake(tcp_encrypted_socket_base* sock)
{
	set_select(IO_NONE);

	encrypted_sock = sock;
	remote_sock.reset(encrypted_sock);
	setup_signal();

	state = ENCRYPTION_HANDSHAKING;
	do_handshake();
}

void net6::connection_base::do_handshake()
{
	if(encrypted_sock == NULL)
	{
		throw std::logic_error(
			"net6::connection_base::do_handshake:\n"
			"No encrypted socket present"
		);
	}

	if(state != ENCRYPTION_HANDSHAKING)
	{
		throw std::logic_error(
			"net6::connection_base::do_handshake:\n"
			"Invalid state"
		);
	}

	if(encrypted_sock->handshake() )
	{
		// Done. Normal select
		sendqueue.unblock();
		io_condition flags = IO_INCOMING | IO_ERROR;
		if(sendqueue.get_size() > 0) flags |= IO_OUTGOING;

		state = ENCRYPTED;
		set_select(flags);
		signal_encrypted.emit();

#ifdef WIN32
		// This is required to call recv as a re-enabling function
		// for asynchronous selection. Note that we cannot just
		// call encrypted_sock->recv here and expect WSAEWOULDBLOCK
		// since real data might be in the buffer that a call could
		// read and we should process.

		// TODO: We should not do this when the
		// socket is in blocking mode!
		do_io(IO_INCOMING);
#endif
	}
	else
	{
		io_condition flags = IO_ERROR;

		// Select depending on current TLS direction
		if(encrypted_sock->get_dir() )
			set_select(flags | IO_OUTGOING);
		else
			set_select(flags | IO_INCOMING);
	}
}

void net6::connection_base::on_send()
{
	if(state == ENCRYPTION_INITIATED_SERVER)
	{
		// All remaining data has been sent, we may now initiate the
		// TLS handshake
		begin_handshake(new tcp_encrypted_socket_server(*remote_sock));
	}
	else
	{
		// All available data has been sent, we need no longer to
		// select for IO_OUTGOING
		net6::io_condition flags = get_select();
		if( (flags & IO_OUTGOING) == IO_OUTGOING)
			set_select(flags & ~IO_OUTGOING);

		signal_send.emit();
	}
}

void net6::connection_base::on_close()
{
	state = CLOSED;

	set_select(IO_NONE);
	sendqueue.clear();
	recvqueue.clear();

	remote_sock.reset(NULL);
	remote_addr.reset(NULL);
	encrypted_sock = NULL;

	signal_close.emit();
}

void net6::connection_base::net_encryption(const packet& pack)
{
	if(state != UNENCRYPTED)
	{
		throw bad_value(
			"Received encryption request in encrypted connection"
		);
	}

	// Received encryption request
	packet reply("net6_encryption_ok");
	send(reply);

	// Block further packets in order to perform a TLS handshake. This
	// is done in on_send(), when all remaining data has been sent.
	sendqueue.block();

	if(pack.get_param(0).as<bool>() == true)
		state = ENCRYPTION_INITIATED_SERVER;
	else
		state = ENCRYPTION_INITIATED_CLIENT;
}

void net6::connection_base::net_encryption_ok(const packet& pack)
{
	if(state != ENCRYPTION_REQUESTED_CLIENT &&
	   state != ENCRYPTION_REQUESTED_SERVER)
	{
		throw bad_value(
			"Received encryption reply without having "
			"requested encryption"
		);
	}

	if(state == ENCRYPTION_REQUESTED_CLIENT)
	{
		begin_handshake(new tcp_encrypted_socket_client(*remote_sock));
	}
	else
	{
		// When we are a server we need to explicitely tell the client
		// when to begin with the TLS handshake. Otherwise the client
		// would send the Client HELLO directly after the
		// net6_encryption_ok. The connection would then read that
		// data into the recvqueue and GnuTLS would wait indefinitely
		// for its client HELLO.

		// We need to prepend this packet because since the queue
		// may already have been filled with other user data.

		// Note that this is still a hack since we cannot "prequeue"
		// a packet, just enqueue it.
		sendqueue.prepend("net6_encryption_begin\n", 22);

		io_condition flags = get_select();
		if( (flags & IO_OUTGOING) == 0)
			set_select(flags | IO_OUTGOING);

		state = ENCRYPTION_INITIATED_SERVER;
	}
}

void net6::connection_base::net_encryption_failed(const packet& pack)
{
	if(state != ENCRYPTION_REQUESTED_CLIENT &&
	   state != ENCRYPTION_REQUESTED_SERVER)
	{
		throw bad_value(
			"Received encryption reply without having "
			"requested encryption"
		);
	}

	sendqueue.unblock();
	state = UNENCRYPTED;

	net6::io_condition flags = net6::IO_INCOMING | net6::IO_ERROR;
	if(sendqueue.get_size() > 0) flags |= net6::IO_OUTGOING;
	set_select(flags);

	signal_encryption_failed.emit();
}

void net6::connection_base::net_encryption_begin(const packet& pack)
{
	if(state != ENCRYPTION_INITIATED_CLIENT)
	{
		throw bad_value(
			"Got encryption_begin without having initiated an "
			"encryption as client."
		);
	}

	begin_handshake(new tcp_encrypted_socket_client(*remote_sock));
}

void net6::connection_base::setup_signal()
{
	remote_sock->io_event().connect(
		sigc::mem_fun(*this, &connection_base::on_sock_event) );
}
