/* object.c
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

#include <stdio.h>

#include "oj.h"
#include "err.h"
#include "parse.h"
#include "resolve.h"
#include "hash.h"

static VALUE
hash_key(ParseInfo pi, const char *key, size_t klen) {
    VALUE	rkey = rb_str_new(key, klen);

#if HAS_ENCODING_SUPPORT
    rb_enc_associate(rkey, oj_utf8_encoding);
#endif
    if (Yes == pi->options.sym_key) {
	rkey = rb_str_intern(rkey);
    }
    return rkey;
}

static int
hat_cstr(ParseInfo pi, Val parent, const char *key, size_t klen, const char *str, size_t len) {
    if (2 == klen) {
	switch (key[1]) {
	case 'o': // object
	    {	// name2class sets and error if the class is not found or created
		VALUE	clas = oj_name2class(pi, str, len, Yes == pi->options.auto_define);

		if (Qundef != clas) {
		    parent->val = rb_obj_alloc(clas);
		}
	    }
	    break;
	case 'c': // class
	    parent->val = oj_name2class(pi, str, len, Yes == pi->options.auto_define);
	    break;
	case 't': // time as a float TBD is a float callback needed
	    // TBD parse time
	case 'u': // ruby struct
	default:
	    return 0;
	    break;
	}
	return 1; // handled
    } else if (3 <= klen && '#' == key[1]) {
	//case 'i': // id in a circular reference
	//case 'r': // ref in a circular reference
	// TBD
    }
    return 0;
}

static int
hat_num(ParseInfo pi, Val parent, const char *key, size_t klen, NumInfo ni) {
    if (2 == klen) {
	switch (key[1]) {
	case 't': // time as a float TBD is a float callback needed
	    {
		int64_t	nsec = ni->num * 1000000000LL / ni->div;

		if (ni->neg) {
		    ni->i = -ni->i;
		    if (0 < nsec) {
			ni->i--;
			nsec = 1000000000LL - nsec;
		    }
		}
#if HAS_NANO_TIME
		parent->val = rb_time_nano_new(ni->i, (long)nsec);
#else
		parent->val = rb_time_new(ni->i, (long)(nsec / 1000));
#endif
	    }
	    break;
	default:
	    return 0;
	    break;
	}
	return 1; // handled
    } else if (3 <= klen && '#' == key[1]) {
	//case 'i': // id in a circular reference
	//case 'r': // ref in a circular reference
	// TBD
    }
    return 0;
}

static int
hat_value(ParseInfo pi, Val parent, const char *key, size_t klen, VALUE value) {
    if (3 <= klen && '#' == key[1]) {
	//case 'i': // id in a circular reference
	//case 'r': // ref in a circular reference
	// TBD
    }
    return 0;
}

static void
set_obj_ivar(VALUE obj, const char *key, size_t klen, VALUE value) {
    ID	var_id;
    ID	*slot;

#if SAFE_CACHE
    pthread_mutex_lock(&oj_cache_mutex);
#endif
    if (0 == (var_id = oj_intern_hash_get(key, klen, &slot))) {
	char	attr[256];

	if (sizeof(attr) <= klen + 2) {
	    char	*buf = ALLOC_N(char, klen + 2);

	    if ('~' == *key) {
		strncpy(buf, key + 1, klen - 1);
		buf[klen - 1] = '\0';
	    } else {
		*buf = '@';
		strncpy(buf + 1, key, klen);
		buf[klen + 1] = '\0';
	    }
	    var_id = rb_intern(buf);
	    xfree(buf);
	} else {
	    if ('~' == *key) {
		strncpy(attr, key + 1, klen - 1);
		attr[klen - 1] = '\0';
	    } else {
		*attr = '@';
		strncpy(attr + 1, key, klen);
		attr[klen + 1] = '\0';
	    }
	    var_id = rb_intern(attr);
	    *slot = var_id;
	}
    }
#if SAFE_CACHE
    pthread_mutex_unlock(&oj_cache_mutex);
#endif

    rb_ivar_set(obj, var_id, value);
}


static void
hash_set_cstr(ParseInfo pi, const char *key, size_t klen, const char *str, size_t len) {
    Val	parent = stack_peek(&pi->stack);

 WHICH_TYPE:
    switch (rb_type(parent->val)) {
    case T_NIL:
	if ('^' != *key || !hat_cstr(pi, parent, key, klen, str, len)) {
	    parent->val = rb_hash_new();
	    goto WHICH_TYPE;
	}
	break;
    case T_HASH:
	{
	    VALUE	rstr = rb_str_new(str, len);
	    VALUE	rkey = rb_str_new(key, klen);

#if HAS_ENCODING_SUPPORT
	    rb_enc_associate(rstr, oj_utf8_encoding);
	    rb_enc_associate(rkey, oj_utf8_encoding);
#endif
	    if (Yes == pi->options.sym_key) {
		rkey = rb_str_intern(rkey);
	    }
	    rb_hash_aset(parent->val, rkey, rstr);
	}
	break;
    case T_OBJECT:
	{
	    VALUE	rstr = rb_str_new(str, len);

#if HAS_ENCODING_SUPPORT
	    rb_enc_associate(rstr, oj_utf8_encoding);
#endif
	    set_obj_ivar(parent->val, key, klen, rstr);
	}
	break;
    default:
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "can not add attributes to a %s", rb_class2name(rb_obj_class(parent->val)));
	return;
    }
}

static void
hash_set_num(ParseInfo pi, const char *key, size_t klen, NumInfo ni) {
    Val	parent = stack_peek(&pi->stack);

 WHICH_TYPE:
    switch (rb_type(parent->val)) {
    case T_NIL:
	if ('^' != *key || !hat_num(pi, parent, key, klen, ni)) {
	    parent->val = rb_hash_new();
	    goto WHICH_TYPE;
	}
	break;
    case T_HASH:
	rb_hash_aset(parent->val, hash_key(pi, key, klen), oj_num_as_value(ni));
	break;
    case T_OBJECT:
	set_obj_ivar(parent->val, key, klen, oj_num_as_value(ni));
	break;
    default:
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "can not add attributes to a %s", rb_class2name(rb_obj_class(parent->val)));
	return;
    }
}

static void
hash_set_value(ParseInfo pi, const char *key, size_t klen, VALUE value) {
    Val	parent = stack_peek(&pi->stack);

 WHICH_TYPE:
    switch (rb_type(parent->val)) {
    case T_NIL:
	if ('^' != *key || !hat_value(pi, parent, key, klen, value)) {
	    parent->val = rb_hash_new();
	    goto WHICH_TYPE;
	}
	break;
    case T_HASH:
	rb_hash_aset(parent->val, hash_key(pi, key, klen), value);
	break;
    case T_OBJECT:
	set_obj_ivar(parent->val, key, klen, value);
	break;
    default:
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "can not add attributes to a %s", rb_class2name(rb_obj_class(parent->val)));
	return;
    }
}


static VALUE
start_hash(ParseInfo pi) {
    return Qnil;
}

static void
end_hash(struct _ParseInfo *pi) {
    Val	parent = stack_peek(&pi->stack);

    if (Qnil == parent->val) {
	parent->val = rb_hash_new();
    }
    // TBD only if parent->val is a Hash
    if (0 != parent->classname) {
	VALUE	clas;

	clas = oj_name2class(pi, parent->classname, parent->clen, 0);
	if (Qundef != clas) { // else an error
	    parent->val = rb_funcall(clas, oj_json_create_id, 1, parent->val);
	} else {
	    char	buf[1024];

	    memcpy(buf, parent->classname, parent->clen);
	    buf[parent->clen] = '\0';
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "class %s is not defined", buf);
	}
	if (parent->classname < pi->json || pi->cur < parent->classname) {
	    xfree((char*)parent->classname);
	    parent->classname = 0;
	}
    }
}

VALUE
oj_object_parse(int argc, VALUE *argv, VALUE self) {
    struct _ParseInfo	pi;

    oj_set_strict_callbacks(&pi);
    pi.end_hash = end_hash;
    pi.start_hash = start_hash;
    pi.hash_set_cstr = hash_set_cstr;
    pi.hash_set_num = hash_set_num;
    pi.hash_set_value = hash_set_value;

    return oj_pi_parse(argc, argv, &pi);
}
