/*
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
 *  Copyright (c) 2013 Window Maker Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef WMSWITCHMENU_H
#define WMSWITCHMENU_H

void switchmenu_additem(WMenu *menu, WWindow *wwin);
void switchmenu_delitem(WMenu *menu, WWindow *wwin);
void switchmenu_handle_notification(WMenu *menu, const char *name, int workspace);
void switchmenu_handle_notification_wwin(WMenu *menu, WWindow *wwin,
					 const char *name, char *data);

WMenu *switchmenu_create(virtual_screen *vscr);
void switchmenu_destroy(virtual_screen *vscr);
void OpenSwitchMenu(virtual_screen *vscr, int x, int y, int keyboard);

#endif /* WMSWITCHMENU_H */
