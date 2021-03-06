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

#ifndef _NET6_ADDRESS_HPP_
#define _NET6_ADDRESS_HPP_

#include <inttypes.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#endif

#include <list>
#include <string>

namespace net6
{

/** Abstract base class representing an internet address
 */
	
class address
{
public:
	address();
	virtual ~address();

	/** Creates a copy of this address. The caller is responsible for
	 * freeing it.
	 */
	virtual address* clone() const = 0;

	/** Returns the address family of this address (AF_INET or AF_INET6).
	 */
	int get_family() const;

	/** Returns a human-readable string of this address
	 * (for example 127.0.0.1, or ::1).
	 */
	virtual std::string get_name() const = 0;

	/** Returns the size of underlaying C sockaddr object.
	 */
	virtual socklen_t get_size() const = 0;

	/** Returns the underlaying C sockaddr object.
	 */
	sockaddr* cobj() { return addr; }
	const sockaddr* cobj() const { return addr; }

protected:
	sockaddr* addr;
};

/** IPv4 internet address
 */

class ipv4_address : public address
{
public:
	static const uint32_t ANY;
	static const uint32_t NONE;
	static const uint32_t BROADCAST;
	static const uint32_t LOOPBACK;

	/** Creates a new IPv4 address of type ANY with port <em>port</em>.
	 */
	ipv4_address(unsigned int port = 0);

	/** Creates a new IPv4 address from a 32 bit integer in network
	 * byte order and with port <em>port</em>.
	 */
	static ipv4_address create_from_address(uint32_t ip_address,
	                                        unsigned int port = 0);

	/** Performs a DNS lookup of the host <em>hostname</em> and stores
	 * its IPv4 address, if any. Otherwise, a net6::error is thrown.
	 */
	static ipv4_address create_from_hostname(const std::string& hostname,
	                                         unsigned int port = 0);

	/** Creates an IPv4 address by copying a C sockaddr object.
	 */
	ipv4_address(const sockaddr_in* other);

	/** Creates a copy of an IPv4 address.
	 */
	ipv4_address(const ipv4_address& other);
	virtual ~ipv4_address();

	/** Performs a DNS lookup of <em>hostname</em> and returns a list
	 * with all addresses found for this host.
	 */
	static std::list<ipv4_address> list(const std::string& hostname,
	                                    unsigned int port = 0);

	/** Copies an IPv4 address.
	 */
	ipv4_address& operator=(const ipv4_address& other);

	/** Assigns a C sockaddr object to this IPv4 address.
	 */
	ipv4_address& operator=(const sockaddr_in* other);

	/** Creates a copy of this IPv4 address on the heap. The caller
	 * is responsible for freeing it.
	 */
	virtual address* clone() const;

	/** Returns a human-readable string of this IPv4 address (for
	 * example 127.0.0.1).
	 */
	virtual std::string get_name() const;

	/** Returns the size of the underlaying C sockaddr_in object.
	 */
	virtual socklen_t get_size() const;

	/** Returns the port assigned to this IPv4 address.
	 */
	unsigned int get_port() const;

	/** Changes the port assigned to this IPv4 address.
	 */
	void set_port(unsigned int port);

	/** Provides access to the underlaying C sockaddr_in object.
	 */
	sockaddr_in* cobj() { return reinterpret_cast<sockaddr_in*>(addr); }
	const sockaddr_in* cobj() const
		{ return reinterpret_cast<sockaddr_in*>(addr); }
protected:
//	ipv4_address();
};

/** IPv6 internet address
 */

class ipv6_address : public address
{
public:
	static const uint8_t ANY[16];
	static const uint8_t LOOPBACK[16];

	/** Creates a new IPv6 address of type ANY and port <em>port</em>.
	 */
	ipv6_address(unsigned int port = 0, unsigned int flowinfo = 0,
	             unsigned int scope_id = 0);

	/** Creates a new IPv6 address from the given byte array in
	 * network byte order and with port <em>port</em>.
	 */
	static ipv6_address create_from_address(const uint8_t ip_address[16],
	                                        unsigned int port = 0,
	                                        unsigned int flowinfo = 0,
	                                        unsigned int scope_id = 0);

	/** Performs a DNS lookup to resolve <em>hostname</em> to an
	 * IPv6 address. If the host could not be resolved, a net6::error is
	 * thrown.
	 */
	static ipv6_address create_from_hostname(const std::string& hostname,
	                                         unsigned int port = 0,
	                                         unsigned int flowinfo = 0,
	                                         unsigned int scope_id = 0);

	/** Creates an IPv6 address from a C sockaddr_in6 object.
	 */
	ipv6_address(const sockaddr_in6* other);

	/** Creates a copy of an IPv6 address.
	 */
	ipv6_address(const ipv6_address& other);
	virtual ~ipv6_address();

	/** Performs a DNS lookup to retrieve a list with all IPv6
	 * addresses of <em>hostname</em>.
	 */
	static std::list<ipv6_address> list(const std::string& hostname,
                                            unsigned int port = 0,
	                                    unsigned int flowinfo = 0,
	                                    unsigned int scope_id = 0);

	/** Creates a copy of an IPv6 address.
	 */
	ipv6_address& operator=(const ipv6_address& other);

	/** Assigns a C sockaddr_in6 object to this IPv6 address object.
	 */
	ipv6_address& operator=(const sockaddr_in6* addr);

	/** Creates a copy of this IPv6 address on the heap. The caller is
	 * responsible for freeing it.
	 */
	virtual address* clone() const;

	/** Returns a human-readable string of the IPv6 address (like ::1).
	 */
	virtual std::string get_name() const;

	/** Returns the size of the underlaying C sockaddr_in6 object.
	 */
	virtual socklen_t get_size() const;

	/** Returns the port associated with this IPv6 address object.
	 */
	unsigned int get_port() const;

	/** Returns the IPv6 flowinfo assigned to this IPv6 address.
	 */
	unsigned int get_flowinfo() const;

	/** Returns the IPv6 scope id assigned to this IPv6 address.
	 */
	unsigned int get_scope_id() const;

	/** Changes the port associated with this IPv6 address object.
	 */
	void set_port(unsigned int port);

	/** Changes the IPv6 flowinfo assigned to this IPv6 address.
	 */
	void set_flowinfo(unsigned int flowinfo);

	/** Changes the IPv6 scope id assigned to this IPv6 address.
	 */
	void set_scope_id(unsigned int scope_id);

	/** Provides access to the underlaying C sockaddr_in6 object.
	 */
	sockaddr_in6* cobj() { return reinterpret_cast<sockaddr_in6*>(addr); }
	const sockaddr_in6* cobj() const
		{ return reinterpret_cast<sockaddr_in6*>(addr); }

protected:
//	ipv6_address();
};

}

#endif
