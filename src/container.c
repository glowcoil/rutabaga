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

#include <stdlib.h>

#include "rutabaga/rutabaga.h"
#include "rutabaga/object.h"
#include "rutabaga/window.h"

static struct rtb_object_implementation super;

/**
 * object implementation
 */

static void realize(rtb_obj_t *self, rtb_obj_t *parent,
		rtb_win_t *window)
{
	super.realize_cb(self, parent, window);
	self->type = rtb_type_ref(window->rtb, self->type,
			"net.illest.rutabaga.container");
}

/**
 * public API
 */

rtb_container_t *rtb_container_new()
{
	rtb_container_t *self = calloc(1, sizeof(*self));

	if (!self)
		return NULL;

	if (rtb_obj_init(self, &super)) {
		free(self);
		return NULL;
	}

	self->realize_cb = realize;

	return self;
}

void rtb_container_add(rtb_container_t *self, rtb_obj_t *obj)
{
	rtb_obj_add_child(self, obj, RTB_ADD_TAIL);
}
