/* resolve.c
 * Copyright (c) 2012, Peter Ohler
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "oj.h"
#include "err.h"
#include "parse.h"

inline static VALUE
resolve_classname(VALUE mod, const char *classname, int auto_define) {
    VALUE	clas;
    ID		ci = rb_intern(classname);

    if (rb_const_defined_at(mod, ci)) {
	clas = rb_const_get_at(mod, ci);
    } else if (auto_define) {
	clas = rb_define_class_under(mod, classname, oj_bag_class);
    } else {
	clas = Qundef;
    }
    return clas;
}

static VALUE
resolve_classpath(ParseInfo pi, const char *name, size_t len) {
    char	class_name[1024];
    VALUE	clas;
    int		auto_define = (Yes == pi->options.auto_define && 0); // TBD
    char	*end = class_name + sizeof(class_name) - 1;
    char	*s;
    const char	*n = name;

    clas = rb_cObject;
    for (s = class_name; 0 < len; n++, len--) {
	if (':' == *n) {
	    *s = '\0';
	    n++;
	    if (':' != *n) {
		//raise_error("Invalid classname, expected another ':'", pi->str, pi->s); // TBD set error
		return Qundef;
	    }
	    if (Qundef == (clas = resolve_classname(clas, class_name, auto_define))) {
		char	buf[256];

		snprintf(buf, sizeof(buf) - 1, "Class %s not defined", class_name);
		//raise_error(buf, pi->str, pi->s);
		return Qundef;
	    }
	    s = class_name;
	} else if (end <= s) {
	    return Qundef;
	    //raise_error("Invalid classname, limit is 1024 characters", pi->str, pi->s);
	} else {
	    *s++ = *n;
	}
    }
    *s = '\0';
    return resolve_classname(clas, class_name, auto_define);
}

VALUE
oj_name2class(ParseInfo pi, const char *name, size_t len) {
    return resolve_classpath(pi, name, len);

#if 0
    VALUE	clas;
    VALUE	*slot;

    if (1 || No == pi->options.class_cache) {
	return resolve_classpath(pi, name);
    }
#if SAFE_CACHE
    pthread_mutex_lock(&oj_cache_mutex);
#endif
    if (Qundef == (clas = oj_cache_get(oj_class_cache, name, &slot))) {
	if (Qundef != (clas = resolve_classpath(pi, name))) {
	    *slot = clas;
	}
    }
#if SAFE_CACHE
    pthread_mutex_unlock(&oj_cache_mutex);
#endif

    return clas;
#endif
}


