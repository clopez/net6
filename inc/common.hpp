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

#ifndef _NET6_COMMON_HPP_
#define _NET6_COMMON_HPP_

#include "gettext_package.hpp"

namespace net6
{

/** Call to initialise gettext and to be able to get translated messages by _()
 */
void init_gettext(gettext_package& package);

/** Translates the given message in the net6 message catalog.
 */
const char* _(const char* msgid);

}

#endif // _NET6_COMMON_HPP_
