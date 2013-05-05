/**
 * rutabaga: an OpenGL widget toolkit
 * Copyright (c) 2013 William Light.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <wctype.h>
#include <sys/time.h>
#include <poll.h>
#include <math.h>

#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlibint.h>

#include "rutabaga/rutabaga.h"
#include "rutabaga/window.h"
#include "rutabaga/event.h"
#include "rutabaga/platform.h"
#include "rutabaga/keyboard.h"

#include "private/util.h"

#include "xrtb.h"

#define FPS 60
#define CAST_EVENT_TO(type) type *ev = (type *) _ev
#define SET_IF_TRUE(w, m, f) (w = (w & ~m) | (-f & m))

/**
 * mouse events
 */

static void handle_mouse_enter(struct xcb_window *win,
		const xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_enter_notify_event_t);
	rtb_platform_mouse_enter_window(RTB_WINDOW(win), ev->event_x, ev->event_y);
}

static void handle_mouse_leave(struct xcb_window *win,
		const xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_leave_notify_event_t);
	rtb_platform_mouse_leave_window(RTB_WINDOW(win), ev->event_x, ev->event_y);
}

static void handle_mouse_button_press(struct xcb_window *win,
		const xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_button_press_event_t);
	xcb_grab_pointer_cookie_t cookie;
	xcb_grab_pointer_reply_t *reply;

	int button;

	switch (ev->detail) {
	case 1: button = RTB_MOUSE_BUTTON1; break;
	case 2: button = RTB_MOUSE_BUTTON2; break;
	case 3: button = RTB_MOUSE_BUTTON3; break;

	default:
		goto dont_handle;
	}

	rtb_platform_mouse_press(RTB_WINDOW(win),
			button, ev->event_x, ev->event_y);

dont_handle:
	cookie = xcb_grab_pointer(
			win->xrtb->xcb_conn, 0, win->xcb_win,
			XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_MOTION |
			XCB_EVENT_MASK_BUTTON_RELEASE,
			XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE,
			XCB_NONE, XCB_CURRENT_TIME);

	if ((reply = xcb_grab_pointer_reply(win->xrtb->xcb_conn, cookie, NULL)))
		free(reply);
}

static void handle_mouse_button_release(struct xcb_window *win,
		const xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_button_release_event_t);
	struct xcb_window *xwin = (void *) win;
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *err;
	int button;

	cookie = xcb_ungrab_pointer_checked(xwin->xrtb->xcb_conn,
			XCB_CURRENT_TIME);

	if ((err = xcb_request_check(xwin->xrtb->xcb_conn, cookie))) {
		ERR("can't ungrab pointer! (%d)\n", err->error_code);
		free(err);
	}

	switch (ev->detail) {
	case 1: button = RTB_MOUSE_BUTTON1; break;
	case 2: button = RTB_MOUSE_BUTTON2; break;
	case 3: button = RTB_MOUSE_BUTTON3; break;

	default:
		return;
	}

	rtb_platform_mouse_release(RTB_WINDOW(win),
			button, ev->event_x, ev->event_y);
}

static void handle_mouse_motion(struct xcb_window *win,
		const xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_motion_notify_event_t);

	rtb_platform_mouse_motion(RTB_WINDOW(win), ev->event_x, ev->event_y);
}

/**
 * keyboard events
 */

static rtb_keycode_t xkeysym_to_rtbkeycode(xcb_keysym_t sym, char32_t *chr)
{
	switch (sym) {
#define CODE(xkey, rtbkey) case xkey: return rtbkey
#define CHAR(xkey, rtbkey, wc) case xkey: *chr = wc; return rtbkey

		CODE(XKB_KEY_Shift_L,     RTB_KEY_LEFT_SHIFT);
		CODE(XKB_KEY_Control_L,   RTB_KEY_LEFT_CTRL);
		CODE(XKB_KEY_Super_L,     RTB_KEY_LEFT_SUPER);
		CODE(XKB_KEY_Alt_L,       RTB_KEY_LEFT_ALT);

		CODE(XKB_KEY_Shift_R,     RTB_KEY_RIGHT_SHIFT);
		CODE(XKB_KEY_Control_R,   RTB_KEY_RIGHT_CTRL);
		CODE(XKB_KEY_Super_R,     RTB_KEY_RIGHT_SUPER);
		CODE(XKB_KEY_Alt_R,       RTB_KEY_RIGHT_ALT);

		CODE(XKB_KEY_Menu,        RTB_KEY_MENU);
		CODE(XKB_KEY_Escape,      RTB_KEY_ESCAPE);

		CODE(XKB_KEY_BackSpace,   RTB_KEY_BACKSPACE);
		CODE(XKB_KEY_Return,      RTB_KEY_RETURN);
		CODE(XKB_KEY_Tab,         RTB_KEY_TAB);
		CODE(XKB_KEY_Caps_Lock,   RTB_KEY_CAPS_LOCK);

		CODE(XKB_KEY_F1,          RTB_KEY_F1);
		CODE(XKB_KEY_F2,          RTB_KEY_F2);
		CODE(XKB_KEY_F3,          RTB_KEY_F3);
		CODE(XKB_KEY_F4,          RTB_KEY_F4);
		CODE(XKB_KEY_F5,          RTB_KEY_F5);
		CODE(XKB_KEY_F6,          RTB_KEY_F6);
		CODE(XKB_KEY_F7,          RTB_KEY_F7);
		CODE(XKB_KEY_F8,          RTB_KEY_F8);
		CODE(XKB_KEY_F9,          RTB_KEY_F9);
		CODE(XKB_KEY_F10,         RTB_KEY_F10);
		CODE(XKB_KEY_F11,         RTB_KEY_F11);
		CODE(XKB_KEY_F12,         RTB_KEY_F12);

		CODE(XKB_KEY_Print,       RTB_KEY_PRINT_SCREEN);
		CODE(XKB_KEY_Pause,       RTB_KEY_SCROLL_LOCK);
		CODE(XKB_KEY_Scroll_Lock, RTB_KEY_SCROLL_LOCK);

		CODE(XKB_KEY_Insert,      RTB_KEY_INSERT);
		CODE(XKB_KEY_Delete,      RTB_KEY_DELETE);
		CODE(XKB_KEY_Home,        RTB_KEY_HOME);
		CODE(XKB_KEY_End,         RTB_KEY_END);
		CODE(XKB_KEY_Prior,       RTB_KEY_PAGE_UP);
		CODE(XKB_KEY_Next,        RTB_KEY_PAGE_DOWN);

		CODE(XKB_KEY_Up,          RTB_KEY_UP);
		CODE(XKB_KEY_Left,        RTB_KEY_LEFT);
		CODE(XKB_KEY_Down,        RTB_KEY_DOWN);
		CODE(XKB_KEY_Right,       RTB_KEY_RIGHT);

		CODE(XKB_KEY_Num_Lock,    RTB_KEY_NUM_LOCK);
		CHAR(XKB_KEY_KP_Divide,   RTB_KEY_NUMPAD,           U'/');
		CHAR(XKB_KEY_KP_Multiply, RTB_KEY_NUMPAD,           U'*');
		CHAR(XKB_KEY_KP_Subtract, RTB_KEY_NUMPAD,           U'-');
		CHAR(XKB_KEY_KP_Add,      RTB_KEY_NUMPAD,           U'+');

		CODE(XKB_KEY_KP_Home,     RTB_KEY_NUMPAD_HOME);
		CODE(XKB_KEY_KP_Up,       RTB_KEY_NUMPAD_UP);
		CODE(XKB_KEY_KP_Prior,    RTB_KEY_NUMPAD_PAGE_UP);
		CODE(XKB_KEY_KP_Left,     RTB_KEY_NUMPAD_LEFT);
		CODE(XKB_KEY_KP_Begin,    RTB_KEY_NUMPAD_MIDDLE);
		CODE(XKB_KEY_KP_Right,    RTB_KEY_NUMPAD_RIGHT);
		CODE(XKB_KEY_KP_End,      RTB_KEY_NUMPAD_END);
		CODE(XKB_KEY_KP_Down,     RTB_KEY_NUMPAD_DOWN);
		CODE(XKB_KEY_KP_Next,     RTB_KEY_NUMPAD_PAGE_DOWN);

		CODE(XKB_KEY_KP_Insert,   RTB_KEY_NUMPAD_INSERT);
		CODE(XKB_KEY_KP_Delete,   RTB_KEY_NUMPAD_INSERT);
		CODE(XKB_KEY_KP_Enter,    RTB_KEY_NUMPAD_ENTER);

#undef CHAR
#undef CODE
	}

	return RTB_KEY_UNKNOWN;
}

static void dispatch_key_event(struct xcb_window *win,
		const xcb_key_press_event_t *ev, rtb_ev_type_t type)
{
	struct rtb_event_key rtb_ev = {.type = type};
	xcb_keysym_t sym;

	sym = xkb_state_key_get_one_sym(win->xrtb->xkb_state, ev->detail);

	/* first, look the keysym up in our internal mod key
	 * translation table. */
	rtb_ev.keycode = xkeysym_to_rtbkeycode(sym, &rtb_ev.character);

	/* if we don't find it there, treat it like an alphanumeric key
	 * and get the UTF-32 value. */
	if (rtb_ev.keycode == RTB_KEY_UNKNOWN) {
		rtb_ev.keycode   = RTB_KEY_NORMAL;
		rtb_ev.character = xkb_keysym_to_utf32(sym);

		if (!rtb_ev.character)
			return;
	}

	rtb_dispatch_raw(RTB_OBJECT(win), RTB_EVENT(&rtb_ev));
}

static int handle_key_press(struct xcb_window *win,
		const xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_key_press_event_t);

	dispatch_key_event(win, ev, RTB_KEY_PRESS);
	xkb_state_update_key(win->xrtb->xkb_state, ev->detail, XKB_KEY_DOWN);
	return 0;
}

static int handle_key_release(struct xcb_window *win,
		const xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_key_release_event_t);

	dispatch_key_event(win, ev, RTB_KEY_RELEASE);
	xkb_state_update_key(win->xrtb->xkb_state, ev->detail, XKB_KEY_UP);
	return 0;
}

static void handle_mapping_notify(struct xcb_window *win,
		xcb_generic_event_t *_ev)
{
	struct xcb_window *xwin = (void *) win;

	xrtb_keyboard_reload(xwin->xrtb);
}

/**
 * window structure events
 */

static void handle_visibility_notify(struct xcb_window *win,
		xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_visibility_notify_event_t);

	switch (ev->state) {
	case XCB_VISIBILITY_UNOBSCURED:
		win->visibility = RTB_UNOBSCURED;
		break;

	case XCB_VISIBILITY_PARTIALLY_OBSCURED:
		win->visibility = RTB_PARTIALLY_OBSCURED;
		break;

	case XCB_VISIBILITY_FULLY_OBSCURED:
		win->visibility = RTB_FULLY_OBSCURED;
		break;
	}
}

static void handle_configure_notify(struct xcb_window *win,
		xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_configure_notify_event_t);

	if (ev->width  != win->w ||
		ev->height != win->h) {
		win->w = ev->width;
		win->h = ev->height;

		win->need_reconfigure = 1;
	}
}

static void handle_client_message(struct xcb_window *win,
		xcb_generic_event_t *_ev)
{
	CAST_EVENT_TO(xcb_client_message_event_t);
	struct xcb_window *xwin = (void *) win;
	struct rtb_event_window rev = {
		.type   = RTB_WINDOW_CLOSE,
		.window = RTB_WINDOW(win)
	};

	if (ev->data.data32[0] == xwin->xrtb->atoms.wm_delete_window)
		rtb_dispatch_raw(RTB_OBJECT(win), RTB_EVENT(&rev));
}

/**
 * ~mystery~ events
 */

static int handle_secret_xlib_event(Display *dpy, xcb_generic_event_t *ev)
{
	unsigned int response_type = ev->response_type & ~0x80;
	int (*proc)(Display *, XEvent *, xEvent *);

	XLockDisplay(dpy);

	proc = XESetWireToEvent(dpy, response_type, 0);
	if (proc) {
		XESetWireToEvent(dpy, response_type, proc);
		XEvent xev;

		ev->sequence = LastKnownRequestProcessed(dpy);
		proc(dpy, &xev, (xEvent *) ev);
	}

	XUnlockDisplay(dpy);

	if (proc)
		return 1;
	return 0;
}

/**
 * actual event loop
 */

int handle_generic_event(struct xcb_window *win,
		xcb_generic_event_t *ev)
{
	switch (ev->response_type & ~0x80) {
	/**
	 * mouse events
	 */

	case XCB_ENTER_NOTIFY:
		handle_mouse_enter(win, ev);
		break;

	case XCB_LEAVE_NOTIFY:
		handle_mouse_leave(win, ev);
		break;

	case XCB_BUTTON_PRESS:
		handle_mouse_button_press(win, ev);
		break;

	case XCB_BUTTON_RELEASE:
		handle_mouse_button_release(win, ev);
		break;

	case XCB_MOTION_NOTIFY:
		handle_mouse_motion(win, ev);
		break;

	/**
	 * keyboard events
	 */

	case XCB_KEY_PRESS:
		handle_key_press(win, ev);
		break;

	case XCB_KEY_RELEASE:
		handle_key_release(win, ev);
		break;

	/**
	 * window structure events
	 */

	case XCB_KEYMAP_NOTIFY:
		/* what */
		break;

	case XCB_MAPPING_NOTIFY:
		handle_mapping_notify(win, ev);
		break;

	case XCB_MAP_NOTIFY:
		break;

	case XCB_EXPOSE:
		break;

	case XCB_VISIBILITY_NOTIFY:
		handle_visibility_notify(win, ev);
		break;

	case XCB_CONFIGURE_NOTIFY:
		handle_configure_notify(win, ev);
		break;

	case XCB_CLIENT_MESSAGE:
		handle_client_message(win, ev);
		break;

	/**
	 * ~mystery~ events
	 */

	default:
		handle_secret_xlib_event(win->xrtb->dpy, ev); /* 2spooky! */
		break;
	}

	return 0;
}

static int drain_xcb_event_queue(rtb_win_t *win, xcb_connection_t *conn)
{
	xcb_generic_event_t *ev;
	int ret;

	while ((ev = xcb_poll_for_event(conn))) {
		ret = handle_generic_event((struct xcb_window *) win, ev);
		free(ev);

		if (ret)
			return -1;
	}

	if (win->need_reconfigure) {
		rtb_window_reinit(win);
		win->need_reconfigure = 0;
	}

	return 0;
}

static void timeval_copy_inc(struct timeval *dst, struct timeval *src,
		int by_usecs)
{
	dst->tv_sec  = src->tv_sec;
	dst->tv_usec = src->tv_usec + by_usecs;

	if (dst->tv_usec > 1000000) {
		dst->tv_sec++;
		dst->tv_usec -= 1000000;
	} else if (dst->tv_usec < 0) {
		dst->tv_sec--;
		dst->tv_usec += 1000000;
	}
}

static int timeval_cmp(struct timeval *a, struct timeval *b)
{
	int diff = 
		((a->tv_sec - b->tv_sec) * 1000000) +
		(a->tv_usec - b->tv_usec);

	if (diff > 0)
		return 1;
	else if (diff < 0)
		return -1;

	return 0;
}

static void timeval_diff(struct timeval *res,
		struct timeval *a, struct timeval *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec;
	res->tv_usec = a->tv_usec - b->tv_usec;
}

#define FRAME_USEC (999999 / FPS)
#define FRAME_MSEC (999 / FPS)

void rtb_event_loop(rtb_t *r)
{
	struct xcb_rutabaga *xrtb = (void *) r;
	struct timeval next_frame, now, diff;
	struct pollfd fds[1];
	rtb_win_t *win = r->win;
	int timeout_ms;

	gettimeofday(&now, NULL);
	timeval_copy_inc(&next_frame, &now, -1);

	r->run_event_loop = 1;

	fds[0].fd = xcb_get_file_descriptor(xrtb->xcb_conn);
	fds[0].events = POLLIN;

	while (r->run_event_loop) {
		gettimeofday(&now, NULL);
		timeval_diff(&diff, &next_frame, &now);
		timeout_ms = (diff.tv_sec * 1000) + (diff.tv_usec / 1000) + 1;
		if (timeout_ms < 0 || timeout_ms > FRAME_MSEC)
			timeout_ms = 1;

		poll(fds, ARRAY_LENGTH(fds), timeout_ms);

		rtb_window_lock(win);

		if (drain_xcb_event_queue(win, xrtb->xcb_conn) < 0)
			return;

		if ((timeval_cmp(&next_frame, &now) < 1) &&
		    win->visibility != RTB_FULLY_OBSCURED) {
			rtb_window_draw(win);
			timeval_copy_inc(&next_frame, &now, FRAME_USEC);
		}

		rtb_window_unlock(win);
	}
}
