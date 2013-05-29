/* parse.c
 * Copyright (c) 2013, Peter Ohler
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
#include <math.h>

#include "oj.h"
#include "parse.h"
#include "buf.h"
#include "val_stack.h"

// Workaround in case INFINITY is not defined in math.h or if the OS is CentOS
#define OJ_INFINITY (1.0/0.0)

#ifdef RUBINIUS_RUBY
#define NUM_MAX 0x07FFFFFF
#else
#define NUM_MAX (FIXNUM_MAX >> 8)
#endif

static void
next_non_white(ParseInfo pi) {
    for (; 1; pi->cur++) {
	switch(*pi->cur) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	    break;
	default:
	    return;
	}
    }
}

static void
skip_comment(ParseInfo pi) {
    if ('*' == *pi->cur) {
	pi->cur++;
	for (; '\0' != *pi->cur; pi->cur++) {
	    if ('*' == *pi->cur && '/' == *(pi->cur + 1)) {
		pi->cur++;
		return;
	    } else if ('\0' == *pi->cur) {
		oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "comment not terminated");
		return;
	    }
	}
    } else if ('/' == *pi->cur) {
	for (; 1; pi->cur++) {
	    switch (*pi->cur) {
	    case '\n':
	    case '\r':
	    case '\f':
	    case '\0':
		return;
	    default:
		break;
	    }
	}
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "invalid comment format");
    }
}

static void
add_value(ParseInfo pi, VALUE rval) {
    Val	parent = stack_peek(&pi->stack);

    if (0 == parent) { // simple add
	if (0 != pi->add_value) {
	    pi->add_value(pi, rval);
	}
    } else {
	switch (parent->next) {
	case NEXT_ARRAY_NEW:
	case NEXT_ARRAY_ELEMENT:
	    if (0 != pi->array_append_value) {
		pi->array_append_value(pi, rval);
	    }
	    parent->next = NEXT_ARRAY_COMMA;
	    break;
	case NEXT_HASH_VALUE:
	    if (0 != pi->hash_set_value) {
		pi->hash_set_value(pi, parent->key, parent->klen, rval);
	    }
	    if (0 != parent->key && (parent->key < pi->json || pi->cur < parent->key)) {
		xfree((char*)parent->key);
		parent->key = 0;
	    }
	    parent->next = NEXT_HASH_COMMA;
	    break;
	case NEXT_HASH_NEW:
	case NEXT_HASH_KEY:
	case NEXT_HASH_COMMA:
	case NEXT_NONE:
	case NEXT_ARRAY_COMMA:
	case NEXT_HASH_COLON:
	default:
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s", oj_stack_next_string(parent->next));
	    break;
	}
    }
}

static void
read_null(ParseInfo pi) {
    if ('u' == *pi->cur++ && 'l' == *pi->cur++ && 'l' == *pi->cur++) {
	add_value(pi, Qnil);
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected null");
    }
}

static void
read_true(ParseInfo pi) {
    if ('r' == *pi->cur++ && 'u' == *pi->cur++ && 'e' == *pi->cur++) {
	add_value(pi, Qtrue);
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected true");
    }
}

static void
read_false(ParseInfo pi) {
    if ('a' == *pi->cur++ && 'l' == *pi->cur++ && 's' == *pi->cur++ && 'e' == *pi->cur++) {
	add_value(pi, Qfalse);
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected false");
    }
}

static uint32_t
read_hex(ParseInfo pi, const char *h) {
    uint32_t	b = 0;
    int		i;

    for (i = 0; i < 4; i++, h++) {
	b = b << 4;
	if ('0' <= *h && *h <= '9') {
	    b += *h - '0';
	} else if ('A' <= *h && *h <= 'F') {
	    b += *h - 'A' + 10;
	} else if ('a' <= *h && *h <= 'f') {
	    b += *h - 'a' + 10;
	} else {
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "invalid hex character");
	    return 0;
	}
    }
    return b;
}

static void
unicode_to_chars(ParseInfo pi, Buf buf, uint32_t code) {
    if (0x0000007F >= code) {
	buf_append(buf, (char)code);
    } else if (0x000007FF >= code) {
	buf_append(buf, 0xC0 | (code >> 6));
	buf_append(buf, 0x80 | (0x3F & code));
    } else if (0x0000FFFF >= code) {
	buf_append(buf, 0xE0 | (code >> 12));
	buf_append(buf, 0x80 | ((code >> 6) & 0x3F));
	buf_append(buf, 0x80 | (0x3F & code));
    } else if (0x001FFFFF >= code) {
	buf_append(buf, 0xF0 | (code >> 18));
	buf_append(buf, 0x80 | ((code >> 12) & 0x3F));
	buf_append(buf, 0x80 | ((code >> 6) & 0x3F));
	buf_append(buf, 0x80 | (0x3F & code));
    } else if (0x03FFFFFF >= code) {
	buf_append(buf, 0xF8 | (code >> 24));
	buf_append(buf, 0x80 | ((code >> 18) & 0x3F));
	buf_append(buf, 0x80 | ((code >> 12) & 0x3F));
	buf_append(buf, 0x80 | ((code >> 6) & 0x3F));
	buf_append(buf, 0x80 | (0x3F & code));
    } else if (0x7FFFFFFF >= code) {
	buf_append(buf, 0xFC | (code >> 30));
	buf_append(buf, 0x80 | ((code >> 24) & 0x3F));
	buf_append(buf, 0x80 | ((code >> 18) & 0x3F));
	buf_append(buf, 0x80 | ((code >> 12) & 0x3F));
	buf_append(buf, 0x80 | ((code >> 6) & 0x3F));
	buf_append(buf, 0x80 | (0x3F & code));
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "invalid Unicode character");
    }
}

// entered at /
static void
read_escaped_str(ParseInfo pi, const char *start) {
    struct _Buf	buf;
    const char	*s;
    int		cnt = pi->cur - start;
    uint32_t	code;
    Val		parent = stack_peek(&pi->stack);

    buf_init(&buf);
    if (0 < cnt) {
	buf_append_string(&buf, start, cnt);
    }
    for (s = pi->cur; '"' != *s; s++) {
	if ('\0' == *s) {
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "quoted string not terminated");
	    buf_cleanup(&buf);
	    return;
	} else if ('\\' == *s) {
	    s++;
	    switch (*s) {
	    case 'n':	buf_append(&buf, '\n');	break;
	    case 'r':	buf_append(&buf, '\r');	break;
	    case 't':	buf_append(&buf, '\t');	break;
	    case 'f':	buf_append(&buf, '\f');	break;
	    case 'b':	buf_append(&buf, '\b');	break;
	    case '"':	buf_append(&buf, '"');	break;
	    case '/':	buf_append(&buf, '/');	break;
	    case '\\':	buf_append(&buf, '\\');	break;
	    case 'u':
		s++;
		if (0 == (code = read_hex(pi, s)) && err_has(&pi->err)) {
		    buf_cleanup(&buf);
		    return;
		}
		s += 3;
		if (0x0000D800 <= code && code <= 0x0000DFFF) {
		    uint32_t	c1 = (code - 0x0000D800) & 0x000003FF;
		    uint32_t	c2;

		    s++;
		    if ('\\' != *s || 'u' != *(s + 1)) {
			pi->cur = s;
			oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "invalid escaped character");
			buf_cleanup(&buf);
			return;
		    }
		    s += 2;
		    if (0 == (c2 = read_hex(pi, s)) && err_has(&pi->err)) {
			buf_cleanup(&buf);
			return;
		    }
		    s += 3;
		    c2 = (c2 - 0x0000DC00) & 0x000003FF;
		    code = ((c1 << 10) | c2) + 0x00010000;
		}
		unicode_to_chars(pi, &buf, code);
		if (err_has(&pi->err)) {
		    buf_cleanup(&buf);
		    return;
		}
		break;
	    default:
		pi->cur = s;
		oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "invalid escaped character");
		buf_cleanup(&buf);
		return;
	    }
	} else {
	    buf_append(&buf, *s);
	}
    }
    if (0 == parent) {
	if (0 != pi->add_cstr) {
	    pi->add_cstr(pi, buf.head, buf_len(&buf));
	}
    } else {
	switch (parent->next) {
	case NEXT_ARRAY_NEW:
	case NEXT_ARRAY_ELEMENT:
	    if (0 != pi->array_append_cstr) {
		pi->array_append_cstr(pi, buf.head, buf_len(&buf));
	    }
	    parent->next = NEXT_ARRAY_COMMA;
	    break;
	case NEXT_HASH_NEW:
	case NEXT_HASH_KEY:
	    // key will not be between pi->json and pi->cur.
	    parent->key = strdup(buf.head); // TBD free this later
	    parent->klen = buf_len(&buf);
	    parent->next = NEXT_HASH_COLON;
	    break;
	case NEXT_HASH_VALUE:
	    if (0 != pi->hash_set_cstr) {
		pi->hash_set_cstr(pi, parent->key, parent->klen, buf.head, buf_len(&buf));
	    }
	    if (0 != parent->key && (parent->key < pi->json || pi->cur < parent->key)) {
		xfree((char*)parent->key);
		parent->key = 0;
	    }
	    parent->next = NEXT_HASH_COMMA;
	    break;
	case NEXT_HASH_COMMA:
	case NEXT_NONE:
	case NEXT_ARRAY_COMMA:
	case NEXT_HASH_COLON:
	default:
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not a string", oj_stack_next_string(parent->next));
	    break;
	}
    }
    pi->cur = s + 1;
    buf_cleanup(&buf);
}

static void
read_str(ParseInfo pi) {
    const char	*str = pi->cur;
    Val		parent = stack_peek(&pi->stack);

    for (; '"' != *pi->cur; pi->cur++) {
	if ('\0' == *pi->cur) {
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "quoted string not terminated");
	    return;
	} else if ('\\' == *pi->cur) {
	    read_escaped_str(pi, str);
	    return;
	}
    }
    if (0 == parent) { // simple add
	if (0 != pi->add_cstr) {
	    pi->add_cstr(pi, str, pi->cur - str);
	}
    } else {
	switch (parent->next) {
	case NEXT_ARRAY_NEW:
	case NEXT_ARRAY_ELEMENT:
	    if (0 != pi->array_append_cstr) {
		pi->array_append_cstr(pi, str, pi->cur - str);
	    }
	    parent->next = NEXT_ARRAY_COMMA;
	    break;
	case NEXT_HASH_NEW:
	case NEXT_HASH_KEY:
	    parent->key = str;
	    parent->klen = pi->cur - str;
	    parent->next = NEXT_HASH_COLON;
	    break;
	case NEXT_HASH_VALUE:
	    if (0 != pi->hash_set_cstr) {
		pi->hash_set_cstr(pi, parent->key, parent->klen, str, pi->cur - str);
	    }
	    if (0 != parent->key && (parent->key < pi->json || pi->cur < parent->key)) {
		xfree((char*)parent->key);
		parent->key = 0;
	    }
	    parent->next = NEXT_HASH_COMMA;
	    break;
	case NEXT_HASH_COMMA:
	case NEXT_NONE:
	case NEXT_ARRAY_COMMA:
	case NEXT_HASH_COLON:
	default:
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not a string", oj_stack_next_string(parent->next));
	    break;
	}
    }
    pi->cur++; // move past "
}

static void
read_num(ParseInfo pi) {
    const char	*start = pi->cur;
    int64_t	n = 0;
    long	a = 0;
    long	div = 1;
    long	e = 0;
    int		neg = 0;
    int		eneg = 0;
    int		big = 0;
    VALUE	rnum = Qnil;
    Val		parent = stack_peek(&pi->stack);

    if ('-' == *pi->cur) {
	pi->cur++;
	neg = 1;
    } else if ('+' == *pi->cur) {
	pi->cur++;
    }
    if ('I' == *pi->cur) {
	if (0 != strncmp("Infinity", pi->cur, 8)) {
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "not a number or other value");
	    return;
	}
	pi->cur += 8;
	if (0 != pi->add_fix) {
	    rnum = (neg) ? rb_float_new(-OJ_INFINITY) : rb_float_new(OJ_INFINITY);
	    add_value(pi, rnum);
	}
	return;
    }
    for (; '0' <= *pi->cur && *pi->cur <= '9'; pi->cur++) {
	if (big) {
	    big++;
	} else {
	    n = n * 10 + (*pi->cur - '0');
	    if (NUM_MAX <= n) {
		big = 1;
	    }
	}
    }
    if ('.' == *pi->cur) {
	pi->cur++;
	for (; '0' <= *pi->cur && *pi->cur <= '9'; pi->cur++) {
	    a = a * 10 + (*pi->cur - '0');
	    div *= 10;
	    if (NUM_MAX <= div) {
		big = 1;
	    }
	}
    }
    if ('e' == *pi->cur || 'E' == *pi->cur) {
	pi->cur++;
	if ('-' == *pi->cur) {
	    pi->cur++;
	    eneg = 1;
	} else if ('+' == *pi->cur) {
	    pi->cur++;
	}
	for (; '0' <= *pi->cur && *pi->cur <= '9'; pi->cur++) {
	    e = e * 10 + (*pi->cur - '0');
	    if (NUM_MAX <= e) {
		big = 1;
	    }
	}
    }
    if (0 != pi->add_fix) { // short cut
	if (Yes == pi->options.bigdec_load) {
	    big = 1;
	}
	if (0 == e && 0 == a && 1 == div) {
	    if (big) {
		int	len = pi->cur - start;
		
		if (256 > len) {
		    char	buf[256];

		    memcpy(buf, start, len);
		    buf[len] = '\0';
		    rnum = rb_cstr_to_inum(buf, 10, 0);
		} else {
		    char	*buf = ALLOC_N(char, len);

		    memcpy(buf, start, len);
		    buf[len] = '\0';
		    rnum = rb_cstr_to_inum(buf, 10, 0);
		    xfree(buf);
		}
	    } else {
		if (neg) {
		    n = -n;
		}
		if (0 == parent) {
		    if (0 != pi->add_fix) {
			pi->add_fix(pi, n);
		    }
		} else {
		    switch (parent->next) {
		    case NEXT_ARRAY_NEW:
		    case NEXT_ARRAY_ELEMENT:
			if (0 != pi->array_append_fix) {
			    pi->array_append_fix(pi, n);
			}
			parent->next = NEXT_ARRAY_COMMA;
			break;
		    case NEXT_HASH_VALUE:
			if (0 != pi->hash_set_fix) {
			    pi->hash_set_fix(pi, parent->key, parent->klen, n);
			}
			if (0 != parent->key && (parent->key < pi->json || pi->cur < parent->key)) {
			    xfree((char*)parent->key);
			    parent->key = 0;
			}
			parent->next = NEXT_HASH_COMMA;
			break;
		    default:
			oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s", oj_stack_next_string(parent->next));
			break;
		    }
		}
		return;
	    }
	} else { /* decimal */
	    if (big) {
		rnum = rb_funcall(oj_bigdecimal_class, oj_new_id, 1, rb_str_new(start, pi->cur - start));
	    } else {
		double	d = (double)n + (double)a / (double)div;

		if (neg) {
		    d = -d;
		}
		if (1 < big) {
		    e += big - 1;
		}
		if (0 != e) {
		    if (eneg) {
			e = -e;
		    }
		    d *= pow(10.0, e);
		}
		rnum = rb_float_new(d);
	    }
	}
	add_value(pi, rnum);
    } else {
	if (0 != parent) {
	    switch (parent->next) {
	    case NEXT_ARRAY_NEW:
	    case NEXT_ARRAY_ELEMENT:
		parent->next = NEXT_ARRAY_COMMA;
		break;
	    case NEXT_HASH_VALUE:
		if (0 != parent->key && (parent->key < pi->json || pi->cur < parent->key)) {
		    xfree((char*)parent->key);
		    parent->key = 0;
		}
		parent->next = NEXT_HASH_COMMA;
		break;
	    default:
		oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s", oj_stack_next_string(parent->next));
		break;
	    }
	}
    }
}

static void
array_start(ParseInfo pi) {
    VALUE	v = Qnil;

    if (0 != pi->start_array) {
	v = pi->start_array(pi);
    }
    stack_push(&pi->stack, v, NEXT_ARRAY_NEW);
}

static void
array_end(ParseInfo pi) {
    Val	array = stack_pop(&pi->stack);

    if (0 == array) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected array close");
    } else if (NEXT_ARRAY_COMMA != array->next && NEXT_ARRAY_NEW != array->next) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not an array close", oj_stack_next_string(array->next));
    } else {
	if (0 != pi->end_array) {
	    pi->end_array(pi);
	}
	add_value(pi, array->val);
    }
}

static void
hash_start(ParseInfo pi) {
    VALUE	v = Qnil;

    if (0 != pi->start_hash) {
	v = pi->start_hash(pi);
    }
    stack_push(&pi->stack, v, NEXT_HASH_NEW);
}

static void
hash_end(ParseInfo pi) {
    Val	hash = stack_pop(&pi->stack);

    if (0 == hash) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected hash close");
    } else if (NEXT_HASH_COMMA != hash->next && NEXT_HASH_NEW != hash->next) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not a hash close", oj_stack_next_string(hash->next));
    } else {
	if (0 != pi->end_hash) {
	    pi->end_hash(pi);
	}
	add_value(pi, hash->val);
    }
}

static void
comma(ParseInfo pi) {
    Val	parent = stack_peek(&pi->stack);

    if (0 == parent) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected comma");
    } else if (NEXT_ARRAY_COMMA == parent->next) {
	parent->next = NEXT_ARRAY_ELEMENT;
    } else if (NEXT_HASH_COMMA == parent->next) {
	parent->next = NEXT_HASH_KEY;
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected comma");
    }
}

static void
colon(ParseInfo pi) {
    Val	parent = stack_peek(&pi->stack);

    if (0 != parent && NEXT_HASH_COLON == parent->next) {
	parent->next = NEXT_HASH_VALUE;
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected colon");
    }
}

void
oj_parse2(ParseInfo pi) {
    pi->cur = pi->json;
    err_init(&pi->err);
    stack_init(&pi->stack);
    while (1) {
	next_non_white(pi);
	switch (*pi->cur++) {
	case '{':
	    hash_start(pi);
	    break;
	case '}':
	    hash_end(pi);
	    break;
	case ':':
	    colon(pi);
	    break;
	case '[':
	    array_start(pi);
	    break;
	case ']':
	    array_end(pi);
	    break;
	case ',':
	    comma(pi);
	    break;
	case '"':
	    read_str(pi);
	    break;
	case '+':
	case '-':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'I':
	    pi->cur--;
	    read_num(pi);
	    break;
	case 't':
	    read_true(pi);
	    break;
	case 'f':
	    read_false(pi);
	    break;
	case 'n':
	    read_null(pi);
	    break;
	case '/':
	    skip_comment(pi);
	    break;
	case '\0':
	    pi->cur--;
	    return;
	default:
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected character");
	    return;
	}
	if (err_has(&pi->err)) {
	    return;
	}
    }
}

void
oj_set_error_at(ParseInfo pi, VALUE err_clas, const char* file, int line, const char *format, ...) {
    va_list	ap;
    char	msg[128];

    va_start(ap, format);
    vsnprintf(msg, sizeof(msg) - 1, format, ap);
    va_end(ap);
    pi->err.clas = err_clas;
    _oj_err_set_with_location(&pi->err, err_clas, msg, pi->json, pi->cur - 1, file, line);
}
