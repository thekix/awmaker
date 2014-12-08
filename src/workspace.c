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
#include "wconfig.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef USE_XSHAPE
#include <X11/extensions/shape.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "WindowMaker.h"
#include "framewin.h"
#include "window.h"
#include "icon.h"
#include "misc.h"
#include "menu.h"
#include "application.h"
#include "dock.h"
#include "actions.h"
#include "workspace.h"
#include "appicon.h"
#include "wmspec.h"
#include "xinerama.h"
#include "event.h"
#include "wsmap.h"

#define MC_NEW          0
#define MC_DESTROY_LAST 1
#define MC_LAST_USED    2
/* index of the first workspace menu entry */
#define MC_WORKSPACE1   3

#define WORKSPACE_NAME_DISPLAY_PADDING 32


static WMPropList *dWorkspaces = NULL;
static WMPropList *dClip, *dName;

static void make_keys(void)
{
	if (dWorkspaces != NULL)
		return;

	dWorkspaces = WMCreatePLString("Workspaces");
	dName = WMCreatePLString("Name");
	dClip = WMCreatePLString("Clip");
}

void wWorkspaceMake(virtual_screen *vscr, int count)
{
	while (count > 0) {
		wWorkspaceNew(vscr);
		count--;
	}
}

static void set_workspace_clip(WDock **clip, virtual_screen *vscr, WMPropList *state) {
	/* We should create and map the dock icon only in the first
	 * workspace, because the image is shared */
	if (!w_global.clip.icon) {
		clip_icon_create();
		clip_icon_map(vscr);
	}

	*clip = clip_create(vscr);
	clip_map(*clip, vscr, state);
}

int wWorkspaceNew(virtual_screen *vscr)
{
	WWorkspace *wspace, **list;
	WMPropList *state;
	static const char *new_name = NULL;
	static size_t name_length;
	int i;
	char *path;

	make_keys();

	/* We need load the WMState file to set the Clip session state */
	path = wdefaultspathfordomain("WMState");
	w_global.session_state = WMReadPropListFromFile(path);
	wfree(path);
	if (!w_global.session_state && w_global.screen_count > 1) {
		path = wdefaultspathfordomain("WMState");
		w_global.session_state = WMReadPropListFromFile(path);
		wfree(path);
	}

	/* Max workspaces reached check */
	if (vscr->workspace.count >= MAX_WORKSPACES)
		return -1;

	/* Create a new one */
	wspace = wmalloc(sizeof(WWorkspace));
	vscr->workspace.count++;

	/* Set the workspace name */
	wspace->name = NULL;
	new_name = _("Workspace %i");
	name_length = strlen(new_name) + 8;
	wspace->name = wmalloc(name_length);
	snprintf(wspace->name, name_length, new_name, vscr->workspace.count);

	/* Set the clip */
	wspace->clip = NULL;
	if (!wPreferences.flags.noclip) {
		state = WMGetFromPLDictionary(w_global.session_state, dClip);
		set_workspace_clip(&wspace->clip, vscr, state);
	}

	list = wmalloc(sizeof(WWorkspace *) * vscr->workspace.count);

	for (i = 0; i < vscr->workspace.count - 1; i++)
		list[i] = vscr->workspace.array[i];

	list[i] = wspace;
	if (vscr->workspace.array)
		wfree(vscr->workspace.array);

	vscr->workspace.array = list;

	wWorkspaceMenuUpdate(vscr, vscr->workspace.menu);
	wWorkspaceMenuUpdate(vscr, vscr->clip.ws_menu);
	wNETWMUpdateDesktop(vscr);
	WMPostNotificationName(WMNWorkspaceCreated, vscr, (void *)(uintptr_t) (vscr->workspace.count - 1));
	XFlush(dpy);

	return vscr->workspace.count - 1;
}

Bool wWorkspaceDelete(virtual_screen *vscr, int workspace)
{
	WWindow *tmp;
	WWorkspace **list;
	int i, j;

	if (workspace <= 0)
		return False;

	/* verify if workspace is in use by some window */
	tmp = vscr->screen_ptr->focused_window;
	while (tmp) {
		if (!IS_OMNIPRESENT(tmp) && tmp->frame->workspace == workspace)
			return False;
		tmp = tmp->prev;
	}

	if (!wPreferences.flags.noclip) {
		wDockDestroy(vscr->workspace.array[workspace]->clip);
		vscr->workspace.array[workspace]->clip = NULL;
	}

	list = wmalloc(sizeof(WWorkspace *) * (vscr->workspace.count - 1));
	j = 0;
	for (i = 0; i < vscr->workspace.count; i++) {
		if (i != workspace) {
			list[j++] = vscr->workspace.array[i];
		} else {
			if (vscr->workspace.array[i]->name)
				wfree(vscr->workspace.array[i]->name);

			if (vscr->workspace.array[i]->map)
				RReleaseImage(vscr->workspace.array[i]->map);

			wfree(vscr->workspace.array[i]);
		}
	}

	wfree(vscr->workspace.array);
	vscr->workspace.array = list;

	vscr->workspace.count--;

	/* update menu */
	wWorkspaceMenuUpdate(vscr, vscr->workspace.menu);
	/* clip workspace menu */
	wWorkspaceMenuUpdate(vscr, vscr->clip.ws_menu);

	/* update also window menu */
	if (vscr->workspace.submenu) {
		WMenu *menu = vscr->workspace.submenu;

		i = menu->entry_no;
		while (i > vscr->workspace.count)
			wMenuRemoveItem(menu, --i);

		wMenuRealize(menu);
	}

	/* and clip menu */
	if (vscr->clip.submenu) {
		WMenu *menu = vscr->clip.submenu;

		i = menu->entry_no;
		while (i > vscr->workspace.count)
			wMenuRemoveItem(menu, --i);

		wMenuRealize(menu);
	}

	wNETWMUpdateDesktop(vscr);
	WMPostNotificationName(WMNWorkspaceDestroyed, vscr, (void *)(uintptr_t) (vscr->workspace.count - 1));

	if (vscr->workspace.current >= vscr->workspace.count)
		wWorkspaceChange(vscr, vscr->workspace.count - 1);

	if (vscr->workspace.last_used >= vscr->workspace.count)
		vscr->workspace.last_used = 0;

	return True;
}

typedef struct WorkspaceNameData {
	int count;
	RImage *back;
	RImage *text;
	time_t timeout;
} WorkspaceNameData;

static void hideWorkspaceName(void *data)
{
	WScreen *scr = (WScreen *) data;

	if (!scr->workspace_name_data || scr->workspace_name_data->count == 0
	    || time(NULL) > scr->workspace_name_data->timeout) {
		XUnmapWindow(dpy, scr->workspace_name);

		if (scr->workspace_name_data) {
			RReleaseImage(scr->workspace_name_data->back);
			RReleaseImage(scr->workspace_name_data->text);
			wfree(scr->workspace_name_data);

			scr->workspace_name_data = NULL;
		}
		scr->workspace_name_timer = NULL;
	} else {
		RImage *img = RCloneImage(scr->workspace_name_data->back);
		Pixmap pix;

		scr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_FADE_DELAY, hideWorkspaceName, scr);

		RCombineImagesWithOpaqueness(img, scr->workspace_name_data->text,
					     scr->workspace_name_data->count * 255 / 10);

		RConvertImage(scr->rcontext, img, &pix);

		RReleaseImage(img);

		XSetWindowBackgroundPixmap(dpy, scr->workspace_name, pix);
		XClearWindow(dpy, scr->workspace_name);
		XFreePixmap(dpy, pix);
		XFlush(dpy);

		scr->workspace_name_data->count--;
	}
}

static void showWorkspaceName(virtual_screen *vscr, int workspace)
{
	WorkspaceNameData *data;
	RXImage *ximg;
	Pixmap text, mask;
	int w, h;
	int px, py;
	char *name = vscr->workspace.array[workspace]->name;
	int len = strlen(name);
	int x, y;
#ifdef USE_XINERAMA
	int head;
	WMRect rect;
	int xx, yy;
#endif

	if (wPreferences.workspace_name_display_position == WD_NONE || vscr->workspace.count < 2)
		return;

	if (vscr->screen_ptr->workspace_name_timer) {
		WMDeleteTimerHandler(vscr->screen_ptr->workspace_name_timer);
		XUnmapWindow(dpy, vscr->screen_ptr->workspace_name);
		XFlush(dpy);
	}

	vscr->screen_ptr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_DELAY, hideWorkspaceName, vscr->screen_ptr);

	if (vscr->screen_ptr->workspace_name_data) {
		RReleaseImage(vscr->screen_ptr->workspace_name_data->back);
		RReleaseImage(vscr->screen_ptr->workspace_name_data->text);
		wfree(vscr->screen_ptr->workspace_name_data);
	}

	data = wmalloc(sizeof(WorkspaceNameData));
	data->back = NULL;

	w = WMWidthOfString(vscr->workspace.font_for_name, name, len);
	h = WMFontHeight(vscr->workspace.font_for_name);

#ifdef USE_XINERAMA
	head = wGetHeadForPointerLocation(vscr);
	rect = wGetRectForHead(vscr->screen_ptr, head);
	if (vscr->screen_ptr->xine_info.count) {
		xx = rect.pos.x + (vscr->screen_ptr->xine_info.screens[head].size.width - (w + 4)) / 2;
		yy = rect.pos.y + (vscr->screen_ptr->xine_info.screens[head].size.height - (h + 4)) / 2;
	} else {
		xx = (vscr->screen_ptr->scr_width - (w + 4)) / 2;
		yy = (vscr->screen_ptr->scr_height - (h + 4)) / 2;
	}
#endif

	switch (wPreferences.workspace_name_display_position) {
	case WD_TOP:
#ifdef USE_XINERAMA
		px = xx;
#else
		px = (scr->scr_width - (w + 4)) / 2;
#endif
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_BOTTOM:
#ifdef USE_XINERAMA
		px = xx;
#else
		px = (vscr->screen_ptr->scr_width - (w + 4)) / 2;
#endif
		py = vscr->screen_ptr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_TOPLEFT:
		px = WORKSPACE_NAME_DISPLAY_PADDING;
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_TOPRIGHT:
		px = vscr->screen_ptr->scr_width - (w + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		py = WORKSPACE_NAME_DISPLAY_PADDING;
		break;
	case WD_BOTTOMLEFT:
		px = WORKSPACE_NAME_DISPLAY_PADDING;
		py = vscr->screen_ptr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_BOTTOMRIGHT:
		px = vscr->screen_ptr->scr_width - (w + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		py = vscr->screen_ptr->scr_height - (h + 4 + WORKSPACE_NAME_DISPLAY_PADDING);
		break;
	case WD_CENTER:
	default:
#ifdef USE_XINERAMA
		px = xx;
		py = yy;
#else
		px = (scr->screen_ptr->scr_width - (w + 4)) / 2;
		py = (scr->screen_ptr->scr_height - (h + 4)) / 2;
#endif
		break;
	}

	XResizeWindow(dpy, vscr->screen_ptr->workspace_name, w + 4, h + 4);
	XMoveWindow(dpy, vscr->screen_ptr->workspace_name, px, py);

	text = XCreatePixmap(dpy, vscr->screen_ptr->w_win, w + 4, h + 4, vscr->screen_ptr->w_depth);
	mask = XCreatePixmap(dpy, vscr->screen_ptr->w_win, w + 4, h + 4, 1);

	XFillRectangle(dpy, text, WMColorGC(vscr->screen_ptr->black), 0, 0, w + 4, h + 4);

	for (x = 0; x <= 4; x++)
		for (y = 0; y <= 4; y++)
			WMDrawString(vscr->screen_ptr->wmscreen, text, vscr->screen_ptr->white, vscr->workspace.font_for_name, x, y, name, len);

	XSetForeground(dpy, vscr->screen_ptr->mono_gc, 1);
	XSetBackground(dpy, vscr->screen_ptr->mono_gc, 0);
	XCopyPlane(dpy, text, mask, vscr->screen_ptr->mono_gc, 0, 0, w + 4, h + 4, 0, 0, 1 << (vscr->screen_ptr->w_depth - 1));
	XSetBackground(dpy, vscr->screen_ptr->mono_gc, 1);
	XFillRectangle(dpy, text, WMColorGC(vscr->screen_ptr->black), 0, 0, w + 4, h + 4);
	WMDrawString(vscr->screen_ptr->wmscreen, text, vscr->screen_ptr->white, vscr->workspace.font_for_name, 2, 2, name, len);

#ifdef USE_XSHAPE
	if (w_global.xext.shape.supported)
		XShapeCombineMask(dpy, vscr->screen_ptr->workspace_name, ShapeBounding, 0, 0, mask, ShapeSet);
#endif
	XSetWindowBackgroundPixmap(dpy, vscr->screen_ptr->workspace_name, text);
	XClearWindow(dpy, vscr->screen_ptr->workspace_name);

	data->text = RCreateImageFromDrawable(vscr->screen_ptr->rcontext, text, None);

	XFreePixmap(dpy, text);
	XFreePixmap(dpy, mask);

	if (!data->text) {
		XMapRaised(dpy, vscr->screen_ptr->workspace_name);
		XFlush(dpy);

		goto erro;
	}

	ximg = RGetXImage(vscr->screen_ptr->rcontext, vscr->screen_ptr->root_win, px, py, data->text->width, data->text->height);
	if (!ximg)
		goto erro;

	XMapRaised(dpy, vscr->screen_ptr->workspace_name);
	XFlush(dpy);

	data->back = RCreateImageFromXImage(vscr->screen_ptr->rcontext, ximg->image, NULL);
	RDestroyXImage(vscr->screen_ptr->rcontext, ximg);

	if (!data->back)
		goto erro;

	data->count = 10;

	/* set a timeout for the effect */
	data->timeout = time(NULL) + 2 + (WORKSPACE_NAME_DELAY + WORKSPACE_NAME_FADE_DELAY * data->count) / 1000;

	vscr->screen_ptr->workspace_name_data = data;

	return;

 erro:
	if (vscr->screen_ptr->workspace_name_timer)
		WMDeleteTimerHandler(vscr->screen_ptr->workspace_name_timer);

	if (data->text)
		RReleaseImage(data->text);

	if (data->back)
		RReleaseImage(data->back);

	wfree(data);

	vscr->screen_ptr->workspace_name_data = NULL;

	vscr->screen_ptr->workspace_name_timer = WMAddTimerHandler(WORKSPACE_NAME_DELAY +
						      10 * WORKSPACE_NAME_FADE_DELAY, hideWorkspaceName, vscr->screen_ptr);
}

void wWorkspaceChange(virtual_screen *vscr, int workspace)
{
	if (w_global.startup.phase1 || w_global.startup.phase2 || vscr->screen_ptr->flags.ignore_focus_events)
		return;

	if (workspace != vscr->workspace.current)
		wWorkspaceForceChange(vscr, workspace);
}

void wWorkspaceRelativeChange(virtual_screen *vscr, int amount)
{
	int w;

	/* While the deiconify animation is going on the window is
	 * still "flying" to its final position and we don't want to
	 * change workspace before the animation finishes, otherwise
	 * the window will land in the new workspace */
	if (vscr->workspace.ignore_change)
		return;

	w = vscr->workspace.current + amount;

	if (amount < 0) {
		if (w >= 0)
			wWorkspaceChange(vscr, w);
		else if (wPreferences.ws_cycle)
			wWorkspaceChange(vscr, vscr->workspace.count + w);
	} else if (amount > 0) {
		if (w < vscr->workspace.count)
			wWorkspaceChange(vscr, w);
		else if (wPreferences.ws_advance)
			wWorkspaceChange(vscr, WMIN(w, MAX_WORKSPACES - 1));
		else if (wPreferences.ws_cycle)
			wWorkspaceChange(vscr, w % vscr->workspace.count);
	}
}

void wWorkspaceForceChange(virtual_screen *vscr, int workspace)
{
	WWindow *tmp, *foc = NULL, *foc2 = NULL;

	if (workspace >= MAX_WORKSPACES || workspace < 0)
		return;

	if (!wPreferences.disable_workspace_pager &&
	    !vscr->workspace.process_map_event)
		wWorkspaceMapUpdate(vscr);

	SendHelperMessage(vscr, 'C', workspace + 1, NULL);

	if (workspace > vscr->workspace.count - 1)
		wWorkspaceMake(vscr, workspace - vscr->workspace.count + 1);

	wClipUpdateForWorkspaceChange(vscr, workspace);

	vscr->workspace.last_used = vscr->workspace.current;
	vscr->workspace.current = workspace;

	wWorkspaceMenuUpdate(vscr, vscr->workspace.menu);
	wWorkspaceMenuUpdate(vscr, vscr->clip.ws_menu);

	if ((tmp = vscr->screen_ptr->focused_window) != NULL) {
		WWindow **toUnmap;
		int toUnmapSize, toUnmapCount;

		if ((IS_OMNIPRESENT(tmp) && (tmp->flags.mapped || tmp->flags.shaded) &&
		     !WFLAGP(tmp, no_focusable)) || tmp->flags.changing_workspace)
			foc = tmp;

		toUnmapSize = 16;
		toUnmapCount = 0;
		toUnmap = wmalloc(toUnmapSize * sizeof(WWindow *));

		/* foc2 = tmp; will fix annoyance with gnome panel
		 * but will create annoyance for every other application
		 */
		while (tmp) {
			if (tmp->frame->workspace != workspace && !tmp->flags.selected) {
				/* unmap windows not on this workspace */
				if ((tmp->flags.mapped || tmp->flags.shaded) &&
				    !IS_OMNIPRESENT(tmp) && !tmp->flags.changing_workspace) {
					if (toUnmapCount == toUnmapSize) {
						toUnmapSize *= 2;
						toUnmap = wrealloc(toUnmap, toUnmapSize * sizeof(WWindow *));
					}

					toUnmap[toUnmapCount++] = tmp;
				}
				/* also unmap miniwindows not on this workspace */
				if (!wPreferences.sticky_icons && tmp->flags.miniaturized &&
				    tmp->icon && !IS_OMNIPRESENT(tmp)) {
					XUnmapWindow(dpy, tmp->icon->core->window);
					tmp->icon->mapped = 0;
				}

				/* update current workspace of omnipresent windows */
				if (IS_OMNIPRESENT(tmp)) {
					WApplication *wapp = wApplicationOf(tmp->main_window);

					tmp->frame->workspace = workspace;

					if (wapp)
						wapp->last_workspace = workspace;

					if (!foc2 && (tmp->flags.mapped || tmp->flags.shaded))
						foc2 = tmp;
				}
			} else {
				/* change selected windows' workspace */
				if (tmp->flags.selected) {
					wWindowChangeWorkspace(tmp, workspace);
					if (!tmp->flags.miniaturized && !foc)
						foc = tmp;

				} else {
					if (!tmp->flags.hidden) {
						if (!(tmp->flags.mapped || tmp->flags.miniaturized)) {
							/* remap windows that are on this workspace */
							wWindowMap(tmp);
							if (!foc && !WFLAGP(tmp, no_focusable))
								foc = tmp;
						}
						/* Also map miniwindow if not omnipresent */
						if (!wPreferences.sticky_icons &&
						    tmp->flags.miniaturized && !IS_OMNIPRESENT(tmp) && tmp->icon) {
							tmp->icon->mapped = 1;
							XMapWindow(dpy, tmp->icon->core->window);
						}
					}
				}
			}
			tmp = tmp->prev;
		}

		while (toUnmapCount > 0)
			wWindowUnmap(toUnmap[--toUnmapCount]);

		wfree(toUnmap);

		/* Gobble up events unleashed by our mapping & unmapping.
		 * These may trigger various grab-initiated focus &
		 * crossing events. However, we don't care about them,
		 * and ignore their focus implications altogether to avoid
		 * flicker.
		 */
		vscr->screen_ptr->flags.ignore_focus_events = 1;
		ProcessPendingEvents();
		vscr->screen_ptr->flags.ignore_focus_events = 0;

		if (!foc)
			foc = foc2;

		if (vscr->screen_ptr->focused_window->flags.mapped && !foc)
			foc = vscr->screen_ptr->focused_window;

		if (wPreferences.focus_mode == WKF_CLICK) {
			wSetFocusTo(vscr, foc);
		} else {
			unsigned int mask;
			int foo;
			Window bar, win;
			WWindow *tmp;

			tmp = NULL;
			if (XQueryPointer(dpy, vscr->screen_ptr->root_win, &bar, &win, &foo, &foo, &foo, &foo, &mask))
				tmp = wWindowFor(win);

			/* If there's a window under the pointer, focus it.
			 * (we ate all other focus events above, so it's
			 * certainly not focused). Otherwise focus last
			 * focused, or the root (depending on sloppiness)
			 */
			if (!tmp && wPreferences.focus_mode == WKF_SLOPPY)
				wSetFocusTo(vscr, foc);
			else
				wSetFocusTo(vscr, tmp);
		}
	}

	/* We need to always arrange icons when changing workspace, even if
	 * no autoarrange icons, because else the icons in different workspaces
	 * can be superposed.
	 * This can be avoided if appicons are also workspace specific.
	 */
	if (!wPreferences.sticky_icons)
		wArrangeIcons(vscr, False);

	if (vscr->dock.dock)
		wAppIconPaint(vscr->dock.dock->icon_array[0]);

	if (!wPreferences.flags.noclip && (vscr->workspace.array[workspace]->clip->auto_collapse ||
					   vscr->workspace.array[workspace]->clip->auto_raise_lower)) {
		/* to handle enter notify. This will also */
		XUnmapWindow(dpy, w_global.clip.icon->icon->core->window);
		XMapWindow(dpy, w_global.clip.icon->icon->core->window);
	} else if (w_global.clip.icon != NULL) {
		wClipIconPaint();
	}

	wScreenUpdateUsableArea(vscr);
	wNETWMUpdateDesktop(vscr);
	showWorkspaceName(vscr, workspace);

	WMPostNotificationName(WMNWorkspaceChanged, vscr, (void *)(uintptr_t) workspace);
}

static void switchWSCommand(WMenu *menu, WMenuEntry *entry)
{
	wWorkspaceChange(menu->frame->vscr, (long)entry->clientdata);
}

static void lastWSCommand(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;

	wWorkspaceChange(menu->frame->vscr, menu->frame->vscr->workspace.last_used);
}

static void deleteWSCommand(WMenu *menu, WMenuEntry *entry)
{
	/* Parameter not used, but tell the compiler that it is ok */
	(void) entry;

	wWorkspaceDelete(menu->frame->vscr, menu->frame->vscr->workspace.count - 1);
}

static void newWSCommand(WMenu *menu, WMenuEntry *foo)
{
	int ws;

	/* Parameter not used, but tell the compiler that it is ok */
	(void) foo;

	ws = wWorkspaceNew(menu->frame->vscr);

	/* autochange workspace */
	if (ws >= 0)
		wWorkspaceChange(menu->frame->vscr, ws);
}

void wWorkspaceRename(virtual_screen *vscr, int workspace, const char *name)
{
	char buf[MAX_WORKSPACENAME_WIDTH + 1];
	char *tmp;

	if (workspace >= vscr->workspace.count)
		return;

	/* trim white spaces */
	tmp = wtrimspace(name);

	if (strlen(tmp) == 0)
		snprintf(buf, sizeof(buf), _("Workspace %i"), workspace + 1);
	else
		strncpy(buf, tmp, MAX_WORKSPACENAME_WIDTH);

	buf[MAX_WORKSPACENAME_WIDTH] = 0;
	wfree(tmp);

	/* update workspace */
	wfree(vscr->workspace.array[workspace]->name);
	vscr->workspace.array[workspace]->name = wstrdup(buf);

	if (vscr->clip.ws_menu) {
		if (strcmp(vscr->clip.ws_menu->entries[workspace + MC_WORKSPACE1]->text, buf) != 0) {
			wfree(vscr->clip.ws_menu->entries[workspace + MC_WORKSPACE1]->text);
			vscr->clip.ws_menu->entries[workspace + MC_WORKSPACE1]->text = wstrdup(buf);
			wMenuRealize(vscr->clip.ws_menu);
		}
	}

	if (vscr->workspace.menu) {
		if (strcmp(vscr->workspace.menu->entries[workspace + MC_WORKSPACE1]->text, buf) != 0) {
			wfree(vscr->workspace.menu->entries[workspace + MC_WORKSPACE1]->text);
			vscr->workspace.menu->entries[workspace + MC_WORKSPACE1]->text = wstrdup(buf);
			wMenuRealize(vscr->workspace.menu);
		}
	}

	if (w_global.clip.icon)
		wClipIconPaint();

	WMPostNotificationName(WMNWorkspaceNameChanged, vscr, (void *)(uintptr_t) workspace);
}

/* callback for when menu entry is edited */
static void onMenuEntryEdited(WMenu *menu, WMenuEntry *entry)
{
	char *tmp;

	tmp = entry->text;
	wWorkspaceRename(menu->frame->vscr, (long)entry->clientdata, tmp);
}

WMenu *wWorkspaceMenuMake(virtual_screen *vscr, Bool titled)
{
	WMenu *wsmenu;
	WMenuEntry *entry;

	wsmenu = wMenuCreate(vscr, titled ? _("Workspaces") : NULL);
	if (!wsmenu) {
		wwarning(_("could not create Workspace menu"));
		return NULL;
	}

	/* callback to be called when an entry is edited */
	wsmenu->on_edit = onMenuEntryEdited;

	wMenuAddCallback(wsmenu, _("New"), newWSCommand, NULL);
	wMenuAddCallback(wsmenu, _("Destroy Last"), deleteWSCommand, NULL);

	entry = wMenuAddCallback(wsmenu, _("Last Used"), lastWSCommand, NULL);
	entry->rtext = GetShortcutKey(wKeyBindings[WKBD_LASTWORKSPACE]);

	return wsmenu;
}

void wWorkspaceMenuUpdate(virtual_screen *vscr, WMenu *menu)
{
	int i;
	long ws;
	char title[MAX_WORKSPACENAME_WIDTH + 1];
	WMenuEntry *entry;
	int tmp;

	if (!menu)
		return;

	if (menu->entry_no < vscr->workspace.count + MC_WORKSPACE1) {
		/* new workspace(s) added */
		i = vscr->workspace.count - (menu->entry_no - MC_WORKSPACE1);
		ws = menu->entry_no - MC_WORKSPACE1;
		while (i > 0) {
			wstrlcpy(title, vscr->workspace.array[ws]->name, MAX_WORKSPACENAME_WIDTH);

			entry = wMenuAddCallback(menu, title, switchWSCommand, (void *)ws);
			entry->flags.indicator = 1;
			entry->flags.editable = 1;

			i--;
			ws++;
		}
	} else if (menu->entry_no > vscr->workspace.count + MC_WORKSPACE1) {
		/* removed workspace(s) */
		for (i = menu->entry_no - 1; i >= vscr->workspace.count + MC_WORKSPACE1; i--)
			wMenuRemoveItem(menu, i);
	}

	for (i = 0; i < vscr->workspace.count; i++) {
		/* workspace shortcut labels */
		if (i / 10 == vscr->workspace.current / 10)
			menu->entries[i + MC_WORKSPACE1]->rtext = GetShortcutKey(wKeyBindings[WKBD_WORKSPACE1 + (i % 10)]);
		else
			menu->entries[i + MC_WORKSPACE1]->rtext = NULL;

		menu->entries[i + MC_WORKSPACE1]->flags.indicator_on = 0;
	}

	menu->entries[vscr->workspace.current + MC_WORKSPACE1]->flags.indicator_on = 1;
	wMenuRealize(menu);

	/* don't let user destroy current workspace */
	if (vscr->workspace.current == vscr->workspace.count - 1)
		wMenuSetEnabled(menu, MC_DESTROY_LAST, False);
	else
		wMenuSetEnabled(menu, MC_DESTROY_LAST, True);

	/* back to last workspace */
	if (vscr->workspace.count && vscr->workspace.last_used != vscr->workspace.current)
		wMenuSetEnabled(menu, MC_LAST_USED, True);
	else
		wMenuSetEnabled(menu, MC_LAST_USED, False);

	tmp = menu->frame->top_width + 5;
	/* if menu got unreachable, bring it to a visible place */
	if (menu->frame_x < tmp - (int)menu->frame->core->width)
		wMenuMove(menu, tmp - (int)menu->frame->core->width, menu->frame_y, False);

	wMenuPaint(menu);
}

void wWorkspaceSaveState(virtual_screen *vscr, WMPropList *old_state)
{
	WMPropList *parr, *pstr, *wks_state, *old_wks_state, *foo, *bar;
	int i;

	make_keys();

	old_wks_state = WMGetFromPLDictionary(old_state, dWorkspaces);
	parr = WMCreatePLArray(NULL);
	for (i = 0; i < vscr->workspace.count; i++) {
		pstr = WMCreatePLString(vscr->workspace.array[i]->name);
		wks_state = WMCreatePLDictionary(dName, pstr, NULL);
		WMReleasePropList(pstr);
		if (!wPreferences.flags.noclip) {
			pstr = wClipSaveWorkspaceState(vscr, i);
			WMPutInPLDictionary(wks_state, dClip, pstr);
			WMReleasePropList(pstr);
		} else if (old_wks_state != NULL) {
			if ((foo = WMGetFromPLArray(old_wks_state, i)) != NULL) {
				if ((bar = WMGetFromPLDictionary(foo, dClip)) != NULL)
					WMPutInPLDictionary(wks_state, dClip, bar);
			}
		}

		WMAddToPLArray(parr, wks_state);
		WMReleasePropList(wks_state);
	}

	WMPutInPLDictionary(w_global.session_state, dWorkspaces, parr);
	WMReleasePropList(parr);
}

void wWorkspaceRestoreState(virtual_screen *vscr)
{
	WMPropList *parr, *pstr, *wks_state, *clip_state;
	int i, j;

	make_keys();

	if (w_global.session_state == NULL)
		return;

	parr = WMGetFromPLDictionary(w_global.session_state, dWorkspaces);

	if (!parr)
		return;

	for (i = 0; i < WMIN(WMGetPropListItemCount(parr), MAX_WORKSPACES); i++) {
		wks_state = WMGetFromPLArray(parr, i);
		if (WMIsPLDictionary(wks_state))
			pstr = WMGetFromPLDictionary(wks_state, dName);
		else
			pstr = wks_state;

		if (i >= vscr->workspace.count)
			wWorkspaceNew(vscr);

		if (vscr->workspace.menu) {
			wfree(vscr->workspace.menu->entries[i + MC_WORKSPACE1]->text);
			vscr->workspace.menu->entries[i + MC_WORKSPACE1]->text = wstrdup(WMGetFromPLString(pstr));
			vscr->workspace.menu->flags.realized = 0;
		}

		wfree(vscr->workspace.array[i]->name);
		vscr->workspace.array[i]->name = wstrdup(WMGetFromPLString(pstr));
		if (!wPreferences.flags.noclip) {
			int added_omnipresent_icons = 0;

			clip_state = WMGetFromPLDictionary(wks_state, dClip);
			if (vscr->workspace.array[i]->clip)
				wDockDestroy(vscr->workspace.array[i]->clip);

			set_workspace_clip(&vscr->workspace.array[i]->clip, vscr, clip_state);

			if (i > 0)
				wDockHideIcons(vscr->workspace.array[i]->clip);

			/* We set the global icons here, because scr->workspaces[i]->clip
			 * was not valid in wDockRestoreState().
			 * There we only set icon->omnipresent to know which icons we
			 * need to set here.
			 */
			for (j = 0; j < vscr->workspace.array[i]->clip->max_icons; j++) {
				WAppIcon *aicon = vscr->workspace.array[i]->clip->icon_array[j];
				int k;

				if (!aicon || !aicon->omnipresent)
					continue;

				aicon->omnipresent = 0;
				if (wClipMakeIconOmnipresent(aicon, True) != WO_SUCCESS)
					continue;

				if (i == 0)
					continue;

				/* Move this appicon from workspace i to workspace 0 */
				vscr->workspace.array[i]->clip->icon_array[j] = NULL;
				vscr->workspace.array[i]->clip->icon_count--;
				added_omnipresent_icons++;

				/* If there are too many omnipresent appicons, we are in trouble */
				assert(vscr->workspace.array[0]->clip->icon_count + added_omnipresent_icons
				       <= vscr->workspace.array[0]->clip->max_icons);

				/* Find first free spot on workspace 0 */
				for (k = 0; k < vscr->workspace.array[0]->clip->max_icons; k++)
					if (vscr->workspace.array[0]->clip->icon_array[k] == NULL)
						break;

				vscr->workspace.array[0]->clip->icon_array[k] = aicon;
				aicon->dock = vscr->workspace.array[0]->clip;
			}

			vscr->workspace.array[0]->clip->icon_count += added_omnipresent_icons;
		}

		WMPostNotificationName(WMNWorkspaceNameChanged, vscr, (void *)(uintptr_t) i);
	}
}

/* Returns the workspace number for a given workspace name */
int wGetWorkspaceNumber(virtual_screen *vscr, const char *value)
{
        int w, i;

	if (sscanf(value, "%i", &w) != 1) {
		w = -1;
		for (i = 0; i < vscr->workspace.count; i++) {
			if (strcmp(vscr->workspace.array[i]->name, value) == 0) {
				w = i;
				break;
			}
		}
	} else {
		w--;
	}

	return w;
}
