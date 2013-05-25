/* val_stack.h
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

#ifndef __OJ_VAL_STACK_H__
#define __OJ_VAL_STACK_H__

#include "ruby.h"

#define STACK_INC	16

typedef enum {
    TYPE_HASH	= 'h',
    TYPE_OBJ	= 'o',
    TYPE_ARRAY	= 'a',
    TYPE_STR	= 's',
    TYPE_BOOL	= 'b',
    TYPE_TIME	= 't',
    TYPE_NUM	= '#',
    TYPE_NULL   = 'n',
    TYPE_ERR	= 'E',
} ValType;

typedef enum {
    NEXT_NONE		= 0,
    NEXT_ARRAY_NEW	= 'a',
    NEXT_ARRAY_ELEMENT	= 'e',
    NEXT_ARRAY_COMMA	= ',',
    NEXT_HASH_NEW	= 'h',
    NEXT_HASH_KEY	= 'k',
    NEXT_HASH_COLON	= ':',
    NEXT_HASH_VALUE	= 'v',
    NEXT_HASH_COMMA	= 'n',
} ValNext;

typedef struct _Val {
    VALUE	val;
    char	type; // ValType
    char	next; // ValNext
} *Val;

typedef struct _ValStack {
    struct _Val	base[STACK_INC];
    Val		head;	// current stack
    Val		end;	// stack end
    Val		tail;	// pointer to one past last element name on stack
} *ValStack;

inline static void
stack_init(ValStack stack) {
    stack->head = stack->base;
    stack->end = stack->base + sizeof(stack->base) / sizeof(struct _Val);
    stack->tail = stack->head;
    stack->head->val = Qundef;
    stack->head->type = TYPE_ERR;
    stack->head->next = NEXT_NONE;
}

inline static int
stack_empty(ValStack stack) {
    return (stack->head == stack->tail);
}

inline static void
stack_cleanup(ValStack stack) {
    if (stack->base != stack->head) {
        xfree(stack->head);
    }
}

inline static void
stack_push(ValStack stack, VALUE val, ValType type) {
    if (stack->end <= stack->tail) {
	size_t	len = stack->end - stack->head;
	size_t	toff = stack->tail - stack->head;

	if (stack->base == stack->head) {
	    stack->head = ALLOC_N(struct _Val, len + STACK_INC);
	    memcpy(stack->head, stack->base, sizeof(struct _Val) * len);
	} else {
	    REALLOC_N(stack->head, struct _Val, len + STACK_INC);
	}
	stack->tail = stack->head + toff;
	stack->end = stack->head + len + STACK_INC;
    }
    stack->tail->val = val;
    stack->tail->type = type;
    switch (type) {
    case TYPE_ARRAY:
	stack->tail->next = NEXT_ARRAY_NEW;
	break;
    case TYPE_HASH:
	stack->tail->next = NEXT_HASH_NEW;
	break;
    default:
	stack->tail->next = NEXT_NONE;
	break;
    }
    stack->tail++;
}

inline static Val
stack_peek(ValStack stack) {
    if (stack->head < stack->tail) {
	return stack->tail - 1;
    }
    return 0;
}

inline static Val
stack_prev(ValStack stack) {
    return stack->tail;
}

inline static Val
stack_pop(ValStack stack) {
    if (stack->head < stack->tail) {
	stack->tail--;
	return stack->tail;
    }
    return 0;
}

inline static ValNext
stack_add_value(ValStack stack, ValType type) {
    if (stack->head < stack->tail) {
	Val	val = stack->tail - 1;

	switch (val->next) {
	case NEXT_ARRAY_NEW:
	case NEXT_ARRAY_ELEMENT:
	    val->next = NEXT_ARRAY_COMMA;
	    break;
	case NEXT_HASH_NEW:
	case NEXT_HASH_KEY:
	    if (TYPE_STR == type) {
		val->next = NEXT_HASH_COLON;
	    } else {
		return NEXT_HASH_KEY;
	    }
	    break;
	case NEXT_HASH_VALUE:
	    val->next = NEXT_HASH_COMMA;
	    break;
	case NEXT_HASH_COMMA:
	case NEXT_NONE:
	case NEXT_ARRAY_COMMA:
	case NEXT_HASH_COLON:
	default:
	    return val->next;
	    break;
	}
    }
    return NEXT_NONE;
}

extern const char*	oj_stack_next_string(ValNext n);

#endif /* __OJ_VAL_STACK_H__ */
