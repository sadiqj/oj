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

// Workaround in case INFINITY is not defined in math.h or if the OS is CentOS
#define OJ_INFINITY (1.0/0.0)

#ifdef RUBINIUS_RUBY
#define NUM_MAX 0x07FFFFFF
#else
#define NUM_MAX (FIXNUM_MAX >> 8)
#endif

#include "oj.h"
#include "parse.h"
#include "val_stack.h"

inline static void
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

inline static void
next_white(ParseInfo pi) {
    for (; 1; pi->cur++) {
	switch(*pi->cur) {
	case ' ':
	case '\t':
	case '\f':
	case '\n':
	case '\r':
	case '\0':
	    return;
	default:
	    break;
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
read_null(ParseInfo pi) {
    ValNext	expected;

    if (NEXT_NONE != (expected = stack_add_value(&pi->stack, TYPE_NULL))) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s", oj_stack_next_string(expected));
    } else if ('u' == *pi->cur++ &&
	'l' == *pi->cur++ &&
	'l' == *pi->cur++) {
	if (0 != pi->add_value) {
	    pi->add_value(pi, Qnil);
	}
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected null");
    }
}

static void
read_true(ParseInfo pi) {
    ValNext	expected;

    if (NEXT_NONE != (expected = stack_add_value(&pi->stack, TYPE_BOOL))) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s", oj_stack_next_string(expected));
    } else if ('r' == *pi->cur++ &&
	       'u' == *pi->cur++ &&
	       'e' == *pi->cur++) {
	if (0 != pi->add_value) {
	    pi->add_value(pi, Qtrue);
	}
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected true");
    }
}

static void
read_false(ParseInfo pi) {
    ValNext	expected;

    if (NEXT_NONE != (expected = stack_add_value(&pi->stack, TYPE_BOOL))) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s", oj_stack_next_string(expected));
    } else if ('a' == *pi->cur++ &&
	       'l' == *pi->cur++ &&
	       's' == *pi->cur++ &&
	       'e' == *pi->cur++) {
	if (0 != pi->add_value) {
	    pi->add_value(pi, Qfalse);
	}
    } else {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected false");
    }
}

static void
read_str(ParseInfo pi) {
    const char	*str = pi->cur;
    ValNext	expected;

    if (NEXT_NONE != (expected = stack_add_value(&pi->stack, TYPE_STR))) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not a string", oj_stack_next_string(expected));
    } else {
	for (; 1; pi->cur++) {
	    switch (*pi->cur) {
	    case '"':
		if (0 != pi->add_value) {
		    VALUE	rstr = rb_str_new(str, pi->cur - str);
		    Val		val = stack_peek(&pi->stack);

#if HAS_ENCODING_SUPPORT
		    rb_enc_associate(rstr, oj_utf8_encoding);
#endif
		    if (0 != val && NEXT_HASH_COLON == val->next) {
			val->val = rstr;
		    }
		    pi->add_value(pi, rstr);
		}
		pi->cur++; // move past "
		return;
	    case '\0':
		oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "quoted string not terminated");
		return;
	    case '\\':
		// TBD drop into read_encoded_str()
		return;
	    default:
		// keep going
		break;
	    }
	}
    }
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
    ValNext	expected;

    if (NEXT_NONE != (expected = stack_add_value(&pi->stack, TYPE_NUM))) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not a number", oj_stack_next_string(expected));
	return;
    }
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
	if (0 != pi->add_value) {
	    VALUE	rnum = (neg) ? rb_float_new(-OJ_INFINITY) : rb_float_new(OJ_INFINITY);

	    pi->add_value(pi, rnum);
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
    if (0 != pi->add_value) {
	VALUE	rnum;

	if (0 == e && 0 == a && 1 == div) {
	    if (big) {
		rnum = rb_funcall(oj_bigdecimal_class, oj_new_id, 1, rb_str_new(start, pi->cur - start));
	    } else {
		if (neg) {
		    n = -n;
		}
		rnum = LONG2NUM(n);
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
	pi->add_value(pi, rnum);
    }
}

static void
array_start(ParseInfo pi) {
    ValNext	expected;

    if (NEXT_NONE != (expected = stack_add_value(&pi->stack, TYPE_ARRAY))) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not an array", oj_stack_next_string(expected));
    } else {
	stack_push(&pi->stack, Qundef, TYPE_ARRAY);
	if (0 != pi->start_array) {
	    pi->start_array(pi);
	}
    }
}

static void
array_end(ParseInfo pi) {
    Val	val = stack_pop(&pi->stack);

    if (NEXT_ARRAY_COMMA != val->next && NEXT_ARRAY_NEW != val->next) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not an array close", oj_stack_next_string(val->next));
    } else {
	if (0 == val || TYPE_ARRAY != val->type) {
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected array close");
	    return;
	}
	if (0 != pi->end_array) {
	    pi->end_array(pi);
	}
    }
}

static void
hash_start(ParseInfo pi) {
    ValNext	expected;

    if (NEXT_NONE != (expected = stack_add_value(&pi->stack, TYPE_HASH))) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not a hash", oj_stack_next_string(expected));
    } else {
	stack_push(&pi->stack, Qundef, TYPE_HASH);
	if (0 != pi->start_hash) {
	    pi->start_hash(pi);
	}
    }
}

static void
hash_end(ParseInfo pi) {
    Val	val = stack_pop(&pi->stack);

    if (NEXT_HASH_COMMA != val->next && NEXT_HASH_NEW != val->next) {
	oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "expected %s, not a hash close", oj_stack_next_string(val->next));
    } else {
	if (0 == val || TYPE_HASH != val->type) {
	    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected hash close");
	    return;
	}
	if (0 != pi->end_hash) {
	    pi->end_hash(pi);
	}
    }
}

static void
comma(ParseInfo pi) {
    Val	val = stack_peek(&pi->stack);

    if (0 != val) {
	switch (val->next) {
	case NEXT_ARRAY_COMMA:
	    val->next = NEXT_ARRAY_ELEMENT;
	    return;
	    break;
	case NEXT_HASH_COMMA:
	    val->next = NEXT_HASH_KEY;
	    return;
	    break;
	default:
	    break;
	}
    }
    oj_set_error_at(pi, oj_parse_error_class, __FILE__, __LINE__, "unexpected comma");
}

static void
colon(ParseInfo pi) {
    Val	val = stack_peek(&pi->stack);

    if (0 != val && NEXT_HASH_COLON == val->next) {
	val->next = NEXT_HASH_VALUE;
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

