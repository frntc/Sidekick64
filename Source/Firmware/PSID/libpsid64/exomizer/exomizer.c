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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "log.h"
#include "search.h"
#include "optimal.h"
#include "output.h"
#include "sfx.h"

static
int
generate_output(match_ctx ctx,
                search_nodep snp,
                struct sfx_decruncher *decr,
                encode_match_f * f,
                encode_match_data emd,
                int load, int len, int start, unsigned char *buf)
{
    int pos;
    int pos_diff;
    int max_diff;
    int diff;
    static output_ctx out;
    output_ctxp old;

    output_ctx_init(out);

    old = emd->out;
    emd->out = out;

    pos = output_get_pos(out);

    pos_diff = pos;
    max_diff = 0;

    output_gamma_code(out, 16);
    output_bits(out, 1, 0); /* 1 bit out */

    diff = output_get_pos(out) - pos_diff;
    if(diff > max_diff)
    {
        max_diff = diff;
    }

    while (snp != NULL)
    {
        const_matchp mp;

        mp = snp->match;
        if (mp != NULL && mp->len > 0)
        {
            if (mp->offset == 0)
            {
                /* literal */
                output_byte(out, ctx->buf[snp->index]);
                output_bits(out, 1, 1);
            } else
            {
                f(mp, emd);
                output_bits(out, 1, 0);
            }

            pos_diff += mp->len;
            diff = output_get_pos(out) - pos_diff;
            if(diff > max_diff)
            {
                max_diff = diff;
            }
        }
        snp = snp->prev;
    }

    /* output header here */
    optimal_out(out, emd);

    output_bits_flush(out);

    output_word(out, (unsigned short int) (load + len));

    len = output_get_pos(out);
    decr->load(out, (unsigned short int) load - max_diff);
    output_copy_bytes(out, 0, len);

    /* second stage of decruncher */
    decr->stages(out, (unsigned short int) start);

    /*len = output_ctx_close(out, of);*/
    len = out->pos - out->start;
    memcpy(buf, out->buf + out->start, len);

    emd->out = old;

    return len;
}

static
search_nodep
do_compress(match_ctx ctx, encode_match_data emd, int max_passes)
{
    matchp_cache_enum mpce;
    matchp_snp_enum snpe;
    search_nodep snp;
    search_nodep best_snp;
    int pass;
    float old_size;

    pass = 1;

    matchp_cache_get_enum(ctx, mpce);
    optimal_optimize(emd, matchp_cache_enum_get_next, mpce);

    best_snp = NULL;
    old_size = 1000000.0;

    for (;;)
    {
        snp = search_buffer(ctx, optimal_encode, emd);
        if (snp == NULL)
        {
            //fprintf(stderr, "error: search_buffer() returned NULL\n");
            //exit(-1);
            return;
        }

        float size = snp->total_score;
        if (size >= old_size)
        {
#if 0 /* RH */
            search_node_free(snp);
#endif /* RH */
            break;
        }

#if 0 /* RH */
        if (best_snp != NULL)
        {
            search_node_free(best_snp);
        }
#endif /* RH */
        best_snp = snp;
        old_size = size;
        ++pass;

        if (pass > max_passes)
        {
            break;
        }

        optimal_free(emd);
        optimal_init(emd);

        matchp_snp_get_enum(snp, snpe);
        optimal_optimize(emd, matchp_snp_enum_get_next, snpe);
    }

    return best_snp;
}


int exomizer(unsigned char *srcbuf, int len, int load, int start, unsigned char *destbuf)
{
    int destlen;
    int max_offset = 65536;
    int max_passes = 65536;
    static match_ctx ctx;
    encode_match_data emd;
    encode_match_priv optimal_priv;
    search_nodep snp;

    match_ctx_init(ctx, srcbuf, len, max_offset);

    emd->out = NULL;
    emd->priv = optimal_priv;

    optimal_init(emd);

    snp = do_compress(ctx, emd, max_passes);

    destlen = generate_output(ctx, snp, sfx_c64ne, optimal_encode, emd,
                              load, len, start, destbuf);
    optimal_free(emd);

#if 0 /* RH */
    search_node_free(snp);
#endif /* RH */
    match_ctx_free(ctx);

    return destlen;
}
