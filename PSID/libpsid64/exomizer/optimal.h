#ifndef ALREADY_INCLUDED_OPTIMAL_H
#define ALREADY_INCLUDED_OPTIMAL_H

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

#include "search.h"
#include "output.h"

float optimal_encode(const_matchp mp,   /* IN */
                     encode_match_data emp);    /* IN */

void optimal_init(encode_match_data emp);       /* IN/OUT */

void optimal_free(encode_match_data emd);       /* IN */

void optimal_optimize(encode_match_data emd,    /* IN/OUT */
                      matchp_enum_get_next_f * f,       /* IN */
                      void *priv);      /* IN */

void optimal_fixup(encode_match_data emd,       /* IN/OUT */
                   int max_len, /* IN */
                   int max_offset);     /* IN */

void optimal_dump(encode_match_data emp);       /* IN */

void optimal_out(output_ctx out,        /* IN/OUT */
                 encode_match_data emd);        /* IN */

#endif
