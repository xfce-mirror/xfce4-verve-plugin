#!/usr/bin/env python
#
# $Id$
#
# vi:set ts=4 sw=4 sts=4 et ai nocindent:
#
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License as 
# published by the Free Software Foundation; either version 2 of the 
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but 
# WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software 
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
# MA 02111-1307 USA

import os
import dbus
import gtk

if __name__ == "__main__":
    # Connect to D-BUS
    bus = dbus.SessionBus()

    # Try to get an org.xfce.RunDialog instance
    remoteObject = bus.get_object("org.xfce.RunDialog", "/org/xfce/RunDialog")

    # Determine home directory
    if os.environ.has_key("HOME"):
        homeDir = os.environ.get("HOME")
    else:
        homeDir = "~/"

    # Determine current display
    display = gtk.gdk.display_get_default()

    # Generate display name
    screen = display.get_default_screen()
    displayName = screen.make_display_name()

    # Call OpenDialog method
    remoteObject.OpenDialog(homeDir, displayName, \
            dbus_interface = "org.xfce.RunDialog")
