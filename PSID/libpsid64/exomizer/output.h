#ifndef ALREADY_INCLUDED_OUTPUT_H
#define ALREADY_INCLUDED_OUTPUT_H

/*
 * Copyright (c) 2002, 2003 Magnus Lind.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software, alter it and re-
 * distribute it freely for any non-commercial, non-profit purpose subject to
 * the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software in a
 *   product, an acknowledgment in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not
 *   be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any distribution.
 *
 *   4. The names of this software and/or it's copyright holders may not be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 */

#include <stdio.h>

struct _output_ctx {
    unsigned int bitbuf;
    unsigned short int pos;
    unsigned short int start;
    unsigned char flags;
    unsigned char buf[65536];
};

typedef struct _output_ctx output_ctx[1];
typedef struct _output_ctx *output_ctxp;

void output_ctx_init(output_ctx ctx);   /* IN/OUT */

void output_ctx_set_start(output_ctx ctx,       /* IN/OUT */
                          unsigned int pos);    /* IN */

void output_ctx_set_reverse(output_ctx ctx);    /* IN/OUT */

unsigned int output_ctx_close(output_ctx ctx,   /* IN */
                              FILE * out);      /* OUT */

unsigned int output_get_pos(output_ctx ctx);    /* IN */

void output_set_pos(output_ctx ctx,     /* IN */
                    unsigned int pos);  /* IN */

void output_copy_bytes(output_ctx ctx,  /* IN */
                       unsigned int src_pos,    /* IN */
                       unsigned int len);       /* IN */

void output_byte(output_ctx ctx,        /* IN/OUT */
                 unsigned char byte);   /* IN */

void output_word(output_ctx ctx,        /* IN/OUT */
                 unsigned short int word);      /* IN */

void output_bits_flush(output_ctx ctx); /* IN/OUT */

void output_bits(output_ctx ctx,        /* IN/OUT */
                 int count,     /* IN */
                 int val);      /* IN */

void output_gamma_code(output_ctx ctx,  /* IN/OUT */
                       int code);       /* IN */
#endif
