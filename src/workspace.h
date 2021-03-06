/* workspace.c- Workspace management
 *
 *  Window Maker window manager
 *
 *  Copyright (c) 1997-2003 Alfredo K. Kojima
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

#ifndef WMWORKSPACE_H_
#define WMWORKSPACE_H_



typedef struct WWorkspace {
	char *name;
	struct WDock *clip;
	RImage *map;
} WWorkspace;

void workspace_create(virtual_screen *vscr);
void workspace_map(virtual_screen *vscr, WWorkspace *wspace, int wksno, WMPropList *parr);

int wGetWorkspaceNumber(virtual_screen *vscr, const char *value);
Bool wWorkspaceDelete(virtual_screen *vscr, int workspace);
void wWorkspaceChange(virtual_screen *vscr, int workspace);
void wWorkspaceForceChange(virtual_screen *vscr, int workspace);
WMenu *wWorkspaceMenuMake(virtual_screen *vscr, Bool titled);
void wWorkspaceMenuUpdate(virtual_screen *vscr, WMenu *menu);
void wWorkspaceMenuUpdate_map(virtual_screen *vscr);
void wWorkspaceMenuEdit(virtual_screen *vscr);
void wWorkspaceSaveState(virtual_screen *vscr, WMPropList *old_state);
void wWorkspaceRename(virtual_screen *vscr, int workspace, const char *name);
void wWorkspaceRelativeChange(virtual_screen *vscr, int amount);

void workspaces_restore(virtual_screen *vscr);
void workspaces_restore_map(virtual_screen *vscr);
void workspaces_set_menu_enabled_items(virtual_screen *vscr, WMenu *menu);
#endif
