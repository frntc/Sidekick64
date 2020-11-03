#ifndef ALREADY_INCLUDED_MATCH_H
#define ALREADY_INCLUDED_MATCH_H
/*
 * Copyright (c) 2002 - 2004 Magnus Lind.
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

#include "chunkpool.h"

struct match {
    unsigned short int offset;
    unsigned short int len;
    struct match *next;
};

typedef struct match match[1];
typedef struct match *matchp;
typedef const struct match *const_matchp;

struct pre_calc {
    struct match_node *single;
    const struct match *cache;
};

typedef struct pre_calc pre_calc[1];

struct match_ctx {
    struct chunkpool m_pool[1];
    pre_calc info[65536];
    unsigned short int rle[65536];
    unsigned short int rle_r[65536];
    const unsigned char *buf;
    int len;
    int max_offset;
};

typedef struct match_ctx match_ctx[1];
typedef struct match_ctx *match_ctxp;

void match_ctx_init(match_ctx ctx,      /* IN/OUT */
                    const unsigned char *buf,   /* IN */
                    int buf_len,        /* IN */
                    int max_offset);    /* IN */

void match_ctx_free(match_ctx ctx);     /* IN/OUT */

/* this needs to be called with the indexes in
 * reverse order */
const_matchp matches_get(match_ctx ctx, /* IN/OUT */
                         unsigned short int index);     /* IN */

void match_delete(match_ctx ctx,        /* IN/OUT */
                  matchp mp);   /* IN */

struct matchp_cache_enum {
    match_ctxp ctx;
    const_matchp next;
    match tmp1;
    match tmp2;
    int pos;
};

typedef struct matchp_cache_enum matchp_cache_enum[1];
typedef struct matchp_cache_enum *matchp_cache_enump;

void matchp_cache_get_enum(match_ctx ctx,       /* IN */
                           matchp_cache_enum mpce);     /* IN/OUT */

typedef const_matchp matchp_enum_get_next_f(void *matchp_enum); /* IN/OUT */

const_matchp matchp_cache_enum_get_next(void *matchp_cache_enum);       /* IN */

#endif
