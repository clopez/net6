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

#ifndef _NET6_CLIENT_HPP_
#define _NET6_CLIENT_HPP_

#include <memory>
#include <sigc++/signal.h>

#include "non_copyable.hpp"
#include "error.hpp"
#include "peer.hpp"
#include "address.hpp"
#include "socket.hpp"
#include "select.hpp"
#include "packet.hpp"
#include "connection.hpp"

namespace net6
{

/** Client in a Client/Server based TCP network system.
 */
	
class client : public sigc::trackable, private non_copyable
{
public:
	/** This class represents a peer in the network.
	 */
	class peer : public net6::peer
	{
	public:
		peer(unsigned int id, const std::string& username);
		~peer();

	protected:
	};

	typedef sigc::signal<void, peer&, const packet&> signal_join_type;
	typedef sigc::signal<void, peer&, const packet&> signal_part_type;
	typedef sigc::signal<void, const packet&> signal_data_type;
	typedef sigc::signal<void> signal_close_type;
	typedef sigc::signal<void, login::error> signal_login_failed_type;
	typedef sigc::signal<void, packet&> signal_login_extend_type;

	/** Creates a new client object which is not connected to anywhere.
	 */
	client();

	/** Creates a new client object and connect to the server at
	 * <em>addr</em>.
	 */
	client(const address& addr);
	virtual ~client();

	/** Connect to the given address. Only use this if the client is not
	 * already connected.
	 */
	void connect(const address& addr);

	/** Disconnects from the server.
	 */
	void disconnect();

	/** Determinates if the client object is connected to a server.
	 */
	bool is_connected() const;

	/** Send a login request with the specified user name. On success,
	 * a join_event with peer==self is emitted, otherwise a
	 * login_failed_event.
	 */
	void login(const std::string& username);

	/** Wait infinitely for incoming network events. Those events are
	 * handled by the client object.
	 */
	virtual void select();

	/** Wait for incoming events or until timeout exceeds. Those events
	 * are handled by the client object.
	 */
	virtual void select(unsigned int timeout);

	/** Send a packet to the network server.
	 */
	virtual void send(const packet& pack);

	/** Look for a peer with the given ID in the network. If there is no
	 * such peer, NULL is returned.
	 */
	peer* find(unsigned int id) const;

	/** Look for a peer with the given user name in the network. If there
	 * is no such peer, NULL is returned.
	 */
	peer* find(const std::string& name) const;

	/** Returns the peer object which represents this host in the network.
	 */
	peer* get_self() const;

	/** Returns the underlaying net6::connection object. Note that this
	 * function causes a segmentation fault when the client is not
	 * connected.
	 */
	const connection& get_connection() const;
	
	/** Signal which is emitted every time a client joins the network.
	 */
	signal_join_type join_event() const;

	/** Signal which is emitted every time a client parts the network.
	 */
	signal_part_type part_event() const;

	/** Signal which is emitted when a packet from the server arrived.
	 */
	signal_data_type data_event() const;

	/** Signal which is emitted when the connection to the server has
	 * been lost. The client will end up in disconnected state after having
	 * received this event.
	 */
	signal_close_type close_event() const;

	/** Signal which is emitted, if a login request failed, for example
	 * if the wished user name was already in use by another client.
	 */
	signal_login_failed_type login_failed_event() const;

	/** Signal which is emitted when a login packet will be sent (most
	 * likely by a call to client::login). It allows the user to append
	 * some more parameters to the login packet which may be evaluated by
	 * the server object in its login_auth and login signals.
	 */
	signal_login_extend_type login_extend_event() const;

protected:
	virtual void on_recv_event(const packet& pack);
	virtual void on_send_event();
	virtual void on_close_event();

	virtual void on_join(peer& client, const packet& pack);
	virtual void on_part(peer& client, const packet& pack);
	virtual void on_data(const packet& pack);
	virtual void on_close();
	virtual void on_login_failed(login::error error);
	virtual void on_login_extend(packet& pack);

	virtual void net_login_failed(const packet& pack);
	virtual void net_client_join(const packet& pack);
	virtual void net_client_part(const packet& pack);

	std::auto_ptr<connection> conn;
	std::list<peer*> peers;
	selector sock_sel;
	peer* self;

	signal_join_type signal_join;
	signal_part_type signal_part;
	signal_data_type signal_data;
	signal_close_type signal_close;
	signal_login_failed_type signal_login_failed;
	signal_login_extend_type signal_login_extend;
};

}

#endif

