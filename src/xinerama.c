/*
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

#include <stdlib.h>

#include "xinerama.h"

#include "screen.h"
#include "window.h"
#include "framewin.h"
#include "placement.h"
#include "dock-core.h"
#include "dock.h"

#ifdef USE_XINERAMA
# ifdef SOLARIS_XINERAMA	/* sucks */
#  include <X11/extensions/xinerama.h>
# else
#  include <X11/extensions/Xinerama.h>
# endif
#endif

static Bool wAppIconTouchesHead(WAppIcon *aicon, int head);

void wInitXinerama(WScreen *scr)
{
	scr->xine_info.primary_head = 0;
	scr->xine_info.screens = NULL;
	scr->xine_info.count = 0;
#ifdef USE_XINERAMA
# ifdef SOLARIS_XINERAMA
	if (XineramaGetState(dpy, scr->screen)) {
		WXineramaInfo *info = &scr->xine_info;
		XRectangle head[MAXFRAMEBUFFERS];
		unsigned char hints[MAXFRAMEBUFFERS];
		int i;

		if (XineramaGetInfo(dpy, scr->screen, head, hints, &info->count)) {

			info->screens = wmalloc(sizeof(WMRect) * (info->count + 1));

			for (i = 0; i < info->count; i++) {
				info->screens[i].pos.x = head[i].x;
				info->screens[i].pos.y = head[i].y;
				info->screens[i].size.width = head[i].width;
				info->screens[i].size.height = head[i].height;
			}
		}
	}
# else				/* !SOLARIS_XINERAMA */
	if (XineramaIsActive(dpy)) {
		XineramaScreenInfo *xine_screens;
		WXineramaInfo *info = &scr->xine_info;
		int i;

		xine_screens = XineramaQueryScreens(dpy, &info->count);

		info->screens = wmalloc(sizeof(WMRect) * (info->count + 1));

		for (i = 0; i < info->count; i++) {
			info->screens[i].pos.x = xine_screens[i].x_org;
			info->screens[i].pos.y = xine_screens[i].y_org;
			info->screens[i].size.width = xine_screens[i].width;
			info->screens[i].size.height = xine_screens[i].height;
		}
		XFree(xine_screens);
	}
# endif				/* !SOLARIS_XINERAMA */
#endif				/* USE_XINERAMA */
}

int wGetRectPlacementInfo(virtual_screen *vscr, WMRect rect, int *flags)
{
	WScreen *scr = vscr->screen_ptr;
	int best;
	unsigned long area, totalArea;
	int i;
	int rx = rect.pos.x;
	int ry = rect.pos.y;
	int rw = rect.size.width;
	int rh = rect.size.height;

	wassertrv(flags != NULL, 0);

	best = -1;
	area = 0;
	totalArea = 0;

	*flags = XFLAG_NONE;

	if (scr->xine_info.count <= 1) {
		unsigned long a;

		a = calcIntersectionArea(rx, ry, rw, rh, 0, 0, scr->scr_width, scr->scr_height);

		if (a == 0)
			*flags |= XFLAG_DEAD;
		else if (a != rw * rh)
			*flags |= XFLAG_PARTIAL;

		return scr->xine_info.primary_head;
	}

	for (i = 0; i < wXineramaHeads(scr); i++) {
		unsigned long a;

		a = calcIntersectionArea(rx, ry, rw, rh,
					 scr->xine_info.screens[i].pos.x,
					 scr->xine_info.screens[i].pos.y,
					 scr->xine_info.screens[i].size.width,
					 scr->xine_info.screens[i].size.height);

		totalArea += a;
		if (a > area) {
			if (best != -1)
				*flags |= XFLAG_MULTIPLE;

			area = a;
			best = i;
		}
	}

	if (best == -1) {
		*flags |= XFLAG_DEAD;
		best = wGetHeadForPointerLocation(vscr);
	} else if (totalArea != rw * rh) {
		*flags |= XFLAG_PARTIAL;
	}

	return best;
}

/* get the head that covers most of the rectangle */
int wGetHeadForRect(virtual_screen *vscr, WMRect rect)
{
	WScreen *scr = vscr->screen_ptr;
	int best, i;
	unsigned long area;
	int rx = rect.pos.x;
	int ry = rect.pos.y;
	int rw = rect.size.width;
	int rh = rect.size.height;

	if (!scr->xine_info.count)
		return scr->xine_info.primary_head;

	best = -1;
	area = 0;

	for (i = 0; i < wXineramaHeads(scr); i++) {
		unsigned long a;

		a = calcIntersectionArea(rx, ry, rw, rh,
					 scr->xine_info.screens[i].pos.x,
					 scr->xine_info.screens[i].pos.y,
					 scr->xine_info.screens[i].size.width,
					 scr->xine_info.screens[i].size.height);

		if (a > area) {
			area = a;
			best = i;
		}
	}

	/* in case rect is in dead space, return valid head */
	if (best == -1)
		best = wGetHeadForPointerLocation(vscr);

	return best;
}

Bool wWindowTouchesHead(WWindow *wwin, int head)
{
	virtual_screen *vscr;
	WMRect rect;
	int a;

	if (!wwin || !wwin->frame)
		return False;

	vscr = wwin->vscr;
	rect = wGetRectForHead(vscr->screen_ptr, head);
	a = calcIntersectionArea(wwin->frame_x, wwin->frame_y,
				 wwin->frame->width, wwin->frame->height,
				 rect.pos.x, rect.pos.y, rect.size.width, rect.size.height);

	return (a != 0);
}

Bool wAppIconTouchesHead(WAppIcon *aicon, int head)
{
	virtual_screen *vscr;
	WMRect rect;
	int a;

	if (!aicon || !aicon->icon)
		return False;

	vscr = aicon->icon->vscr;
	rect = wGetRectForHead(vscr->screen_ptr, head);
	a = calcIntersectionArea(aicon->x_pos, aicon->y_pos,
				 aicon->icon->width, aicon->icon->height,
				 rect.pos.x, rect.pos.y, rect.size.width, rect.size.height);

	return (a != 0);
}

int wGetHeadForWindow(WWindow *wwin)
{
	WMRect rect;

	if (wwin == NULL || wwin->frame == NULL)
		return 0;

	rect.pos.x = wwin->frame_x;
	rect.pos.y = wwin->frame_y;
	rect.size.width = wwin->frame->width;
	rect.size.height = wwin->frame->height;

	return wGetHeadForRect(wwin->vscr, rect);
}

int wGetHeadForPoint(virtual_screen *vscr, WMPoint point)
{
	WScreen *scr = vscr->screen_ptr;
	int i;

	for (i = 0; i < scr->xine_info.count; i++) {
		WMRect *rect = &scr->xine_info.screens[i];

		if ((unsigned)(point.x - rect->pos.x) < rect->size.width &&
		    (unsigned)(point.y - rect->pos.y) < rect->size.height)
			return i;
	}

	return scr->xine_info.primary_head;
}

int wGetHeadForPointerLocation(virtual_screen *vscr)
{
	WScreen *scr = vscr->screen_ptr;
	WMPoint point;
	Window bla;
	int ble;
	unsigned int blo;

	if (!scr->xine_info.count)
		return scr->xine_info.primary_head;

	if (!XQueryPointer(dpy, scr->root_win, &bla, &bla, &point.x, &point.y, &ble, &ble, &blo))
		return scr->xine_info.primary_head;

	return wGetHeadForPoint(vscr, point);
}

/* get the dimensions of the head */
WMRect wGetRectForHead(WScreen *scr, int head)
{
	WMRect rect;

	if (head < scr->xine_info.count) {
		rect.pos.x = scr->xine_info.screens[head].pos.x;
		rect.pos.y = scr->xine_info.screens[head].pos.y;
		rect.size.width = scr->xine_info.screens[head].size.width;
		rect.size.height = scr->xine_info.screens[head].size.height;
	} else {
		rect.pos.x = 0;
		rect.pos.y = 0;
		rect.size.width = scr->scr_width;
		rect.size.height = scr->scr_height;
	}

	return rect;
}

WArea wGetUsableAreaForHead(virtual_screen *vscr, int head, WArea *totalAreaPtr, Bool noicons)
{
	WScreen *scr = vscr->screen_ptr;
	WArea totalArea, usableArea;
	WMRect rect = wGetRectForHead(vscr->screen_ptr, head);

	totalArea.x1 = rect.pos.x;
	totalArea.y1 = rect.pos.y;
	totalArea.x2 = totalArea.x1 + rect.size.width;
	totalArea.y2 = totalArea.y1 + rect.size.height;

	if (totalAreaPtr != NULL)
		*totalAreaPtr = totalArea;

	if (head < wXineramaHeads(scr))
		usableArea = noicons ? scr->totalUsableArea[head] : scr->usableArea[head];
	else
		usableArea = totalArea;

	if (noicons) {
		/* check if user wants dock covered */
		if (vscr->dock.dock && wPreferences.no_window_over_dock &&
		    wAppIconTouchesHead(vscr->dock.dock->icon_array[0], head)) {
			int offset = wPreferences.icon_size + DOCK_EXTRA_SPACE;

			if (vscr->dock.dock->on_right_side)
				usableArea.x2 -= offset;
			else
				usableArea.x1 += offset;
		}

		/* check if icons are on the same side as dock, and adjust if not done already */
		if (vscr->dock.dock && wPreferences.no_window_over_icons &&
		    !wPreferences.no_window_over_dock && (wPreferences.icon_yard & IY_VERT)) {
			int offset = wPreferences.icon_size + DOCK_EXTRA_SPACE;

			if (vscr->dock.dock->on_right_side && (wPreferences.icon_yard & IY_RIGHT))
				usableArea.x2 -= offset;

			/* can't use IY_LEFT in if, it's 0 ... */
			if (!vscr->dock.dock->on_right_side && !(wPreferences.icon_yard & IY_RIGHT))
				usableArea.x1 += offset;
		}
	}

	return usableArea;
}

WMPoint wGetPointToCenterRectInHead(virtual_screen *vscr, int head, int width, int height)
{
	WMPoint p;
	WMRect rect = wGetRectForHead(vscr->screen_ptr, head);

	p.x = rect.pos.x + (rect.size.width - width) / 2;
	p.y = rect.pos.y + (rect.size.height - height) / 2;

	return p;
}

/*
 * Find head on left, right, up or down direction relative to current
 * head. If there is no screen available on pointed direction, -1 will be
 * returned.*/
int wGetHeadRelativeToCurrentHead(virtual_screen *vscr, int current_head, int direction)
{
	short int found = 0;
	int i;
	int distance = 0;
	int smallest_distance = 0;
	WScreen *scr = vscr->screen_ptr;
	int nearest_head = scr->xine_info.primary_head;
	WMRect crect = wGetRectForHead(scr, current_head);

	for (i = 0; i < scr->xine_info.count; i++) {
		if (i == current_head)
			continue;

		WMRect *rect = &scr->xine_info.screens[i];

		/* calculate distance from the next screen to current one */
		switch (direction) {
			case DIRECTION_LEFT:
				if (rect->pos.x < crect.pos.x) {
					found = 1;
					distance = abs((rect->pos.x + rect->size.width)
							- crect.pos.x) + abs(rect->pos.y + crect.pos.y);
				}
				break;
			case DIRECTION_RIGHT:
				if (rect->pos.x > crect.pos.x) {
					found = 1;
					distance = abs((crect.pos.x + crect.size.width)
							- rect->pos.x) + abs(rect->pos.y + crect.pos.y);
				}
				break;
			case DIRECTION_UP:
				if (rect->pos.y < crect.pos.y) {
					found = 1;
					distance = abs((rect->pos.y + rect->size.height)
							- crect.pos.y) + abs(rect->pos.x + crect.pos.x);
				}
				break;
			case DIRECTION_DOWN:
				if (rect->pos.y > crect.pos.y) {
					found = 1;
					distance = abs((crect.pos.y + crect.size.height)
							- rect->pos.y) + abs(rect->pos.x + crect.pos.x);
				}
				break;
		}

		if (found && distance == 0)
			return i;

		if (smallest_distance == 0)
			smallest_distance = distance;

		if (abs(distance) <= smallest_distance) {
			smallest_distance = distance;
			nearest_head = i;
		}
	}

	if (found && smallest_distance != 0 && nearest_head != current_head)
		return nearest_head;

	return -1;
}
