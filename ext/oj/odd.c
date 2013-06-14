/* odd.c
 * Copyright (c) 2011, Peter Ohler
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 *  - Neither the name of Peter Ohler nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "odd.h"

struct _Odd	odds[5]; // bump up if new Odd classes are added

void
oj_odd_init() {
    Odd	odd;
    ID	*idp;
    ID	new_id = rb_intern("new");

    odd = odds;
    // Rational
    idp = odd->attrs;
    odd->classname = "Rational";
    odd->clas = rb_const_get(rb_cObject, rb_intern("Rational"));
    odd->create_obj = rb_cObject;
    odd->create_op = rb_intern("Rational");
    odd->attr_cnt = 2;
    *idp++ = rb_intern("numerator");
    *idp++ = rb_intern("denominator");
    *idp++ = 0;
    // Date
    odd++;
    idp = odd->attrs;
    odd->classname = "Date";
    odd->clas = rb_const_get(rb_cObject, rb_intern("Date"));
    odd->create_obj = odd->clas;
    odd->create_op = new_id;
    odd->attr_cnt = 4;
    *idp++ = rb_intern("year");
    *idp++ = rb_intern("month");
    *idp++ = rb_intern("day");
    *idp++ = rb_intern("start");
    *idp++ = 0;
    // DateTime
    odd++;
    idp = odd->attrs;
    odd->classname = "DateTime";
    odd->clas = rb_const_get(rb_cObject, rb_intern("DateTime"));
    odd->create_obj = odd->clas;
    odd->create_op = new_id;
    odd->attr_cnt = 8;
    *idp++ = rb_intern("year");
    *idp++ = rb_intern("month");
    *idp++ = rb_intern("day");
    *idp++ = rb_intern("hour");
    *idp++ = rb_intern("min");
    *idp++ = rb_intern("sec");
    *idp++ = rb_intern("offset");
    *idp++ = rb_intern("start");
    *idp++ = 0;
    // Range
    odd++;
    idp = odd->attrs;
    odd->classname = "Range";
    odd->clas = rb_const_get(rb_cObject, rb_intern("Range"));
    odd->create_obj = odd->clas;
    odd->create_op = new_id;
    odd->attr_cnt = 3;
    *idp++ = rb_intern("begin");
    *idp++ = rb_intern("end");
    *idp++ = rb_intern("exclude_end?");
    *idp++ = 0;
    // The end. bump up the size of odds if a new class is added.
    odd++;
    odd->clas = Qundef;
}

Odd
oj_get_odd(VALUE clas) {
    Odd	odd = odds;

    for (; Qundef != odd->clas; odd++) {
	if (clas == odd->clas) {
	    return odd;
	}
    }
    return 0;
}
