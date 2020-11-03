#ifndef ALREADY_INCLUDED_RADIX_H
#define ALREADY_INCLUDED_RADIX_H
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

#include "chunkpool.h"

typedef struct _radix_node *radix_nodep;

struct _radix_root {
    int depth;
    radix_nodep root;
    struct chunkpool mem[1];
};

typedef struct _radix_root radix_root[1];
typedef struct _radix_root *radix_rootp;


typedef void free_callback(void *data, void *priv);

/* *f will be called even for null pointers */
void radix_tree_free(radix_root rr,     /* IN */
                     free_callback * f, /* IN */
                     void *priv);       /* IN */

void radix_tree_init(radix_root rr);    /* IN */

void radix_node_set(radix_root rr,      /* IN */
                    unsigned int index, /* IN */
                    void *data);        /* IN */

void *radix_node_get(radix_root rr,     /* IN */
                     unsigned int index);       /* IN */

#endif
