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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "log.h"
#include "match.h"
#include "chunkpool.h"
#include "radix.h"


struct match_node {
    int index;
    struct match_node *next;
};

static
const_matchp matches_calc(match_ctx ctx,        /* IN/OUT */
                          unsigned short int index);    /* IN */

matchp match_new(match_ctx ctx, /* IN/OUT */
                 matchp *mpp,
                 unsigned short int len,
                 unsigned short int offset)
{
    matchp m = chunkpool_malloc(ctx->m_pool);
    m->len = len;
    m->offset = offset;

    /* insert new node in list */
    m->next = *mpp;
    *mpp = m;

    return m;
}


void match_ctx_init(match_ctx ctx,      /* IN/OUT */
                    const unsigned char *buf,      /* IN */
                    int buf_len,        /* IN */
                    int max_offset)
{
    struct match_node *np;
    struct chunkpool map_pool[1];

    int c, i;
    int val;

    memset(ctx->info, 0, sizeof(ctx->info));
    memset(ctx->rle, 0, sizeof(ctx->rle));
    memset(ctx->rle_r, 0, sizeof(ctx->rle_r));

    chunkpool_init(ctx->m_pool, sizeof(match));
    chunkpool_init(map_pool, sizeof(match));

    ctx->max_offset = max_offset;

    ctx->buf = buf;
    ctx->len = buf_len;

    val = buf[0];
    for (i = 1; i < buf_len; ++i)
    {
        if (buf[i] == val)
        {
            ctx->rle[i] = ctx->rle[i - 1] + 1;
        } else
        {
            ctx->rle[i] = 0;
        }
        val = buf[i];
    }

    val = buf[buf_len - 1];
    for (i = buf_len - 2; i >= 0; --i)
    {
        if (buf[i] == val)
        {
            ctx->rle_r[i] = ctx->rle_r[i + 1] + 1;
        } else
        {
            ctx->rle_r[i] = 0;
        }
        val = buf[i];
    }

    /* add extra nodes to rle sequences */
    for(c = 0; c < 256; ++c)
    {
        static char rle_map[65536];
        struct match_node *prev_np;
        int rle_len;

        /* for each possible rle char */
        memset(rle_map, 0, sizeof(rle_map));
        prev_np = NULL;
        for (i = 0; i < buf_len; ++i)
        {
            /* must be the correct char */
            if(buf[i] != c)
            {
                continue;
            }

            rle_len = ctx->rle[i];
            if(!rle_map[rle_len] && ctx->rle_r[i] > 16)
            {
                /* no previous lengths and not our primary length*/
                continue;
            }

            np = chunkpool_malloc(ctx->m_pool);
            np->index = i;
            np->next = NULL;
            rle_map[rle_len] = 1;

            LOG(LOG_DEBUG, ("0) c = %d, added np idx %d -> %d\n", c, i, 0));

            /* if we have a previous entry, let's chain it together */
            if(prev_np != NULL)
            {
                LOG(LOG_DEBUG, ("1) c = %d, pointed np idx %d -> %d\n",
                                c, prev_np->index, i));
                prev_np->next = np;
            }

            ctx->info[i]->single = np;
            prev_np = np;
        }

        memset(rle_map, 0, sizeof(rle_map));
        prev_np = NULL;
        for (i = buf_len - 1; i >= 0; --i)
        {
            /* must be the correct char */
            if(buf[i] != c)
            {
                continue;
            }

            rle_len = ctx->rle_r[i];
            np = ctx->info[i]->single;
            if(np == NULL)
            {
                if(rle_map[rle_len] && prev_np != NULL && rle_len > 0)
                {
                    np = chunkpool_malloc(ctx->m_pool);
                    np->index = i;
                    np->next = prev_np;
                    ctx->info[i]->single = np;

                    LOG(LOG_DEBUG, ("2) c = %d, added np idx %d -> %d\n",
                                    c, i, prev_np->index));
                }
            }
            else
            {
                prev_np = np;
            }

            if(ctx->rle_r[i] > 0)
            {
                continue;
            }
            rle_len = ctx->rle[i] + 1;
            rle_map[rle_len] = 1;
        }
    }

    for (i = buf_len - 1; i >= 0; --i)
    {
        const_matchp matches;
        /* let's populate the cache */
        matches = matches_calc(ctx, (unsigned short) i);

        /* add to cache */
        ctx->info[i]->cache = matches;

        if (!(i & 0xFF))
        {
            LOG(LOG_NORMAL, ("."));
        }

    }

    LOG(LOG_NORMAL, ("\n"));

    chunkpool_free(map_pool);
}

void match_ctx_free(match_ctx ctx)      /* IN/OUT */
{
    chunkpool_free(ctx->m_pool);
}

void dump_matches(matchp mp)
{
    if (mp == NULL)
    {
        LOG(LOG_DEBUG, (" (NULL)\n"));
    } else
    {
        if(mp->offset > 0)
        {
            LOG(LOG_DEBUG, (" offset %d, len %d\n", mp->offset, mp->len));
        }
        if (mp->next != NULL)
        {
            dump_matches(mp->next);
        }
    }
}

const_matchp matches_get(match_ctx ctx, /* IN/OUT */
                         unsigned short int index)      /* IN */
{
    return ctx->info[index]->cache;

}

/* this needs to be called with the indexes in
 * reverse order */
const_matchp matches_calc(match_ctx ctx,        /* IN/OUT */
                          unsigned short int index)     /* IN */
{
    const unsigned char *buf;

    matchp matches;
    matchp mp;
    struct match_node *np;

    buf = ctx->buf;
    matches = NULL;

    LOG(LOG_DUMP, ("index %d, char '%c', rle %d, rle_r %d\n",
                   index, buf[index], ctx->rle[index],
                   ctx->rle_r[index]));

    /* proces the literal match and add it to matches */
    mp = match_new(ctx, &matches, 1, 0);

    /* get possible match */
    np = ctx->info[index]->single;
    if(np != NULL)
    {
        np = np->next;
    }
    for (; np != NULL; np = np->next)
    {
        int mp_len;
        int len;
        int pos;
        int offset;

        /* limit according to max offset */
        if(np->index > index + ctx->max_offset)
        {
            break;
        }

        LOG(LOG_DEBUG, ("find lengths for index %d to index %d\n",
                        index, np->index));

        /* get match len */
        mp_len = mp->offset > 0 ? mp->len : 0;
        LOG(LOG_DEBUG, ("0) comparing with current best [%d] off %d len %d\n",
                        index, mp->offset, mp_len));

        offset = np->index - index;
        len = mp_len;
        pos = index + 1 - len;
        /* Compare the first <previous len> bytes backwards. We can
         * skip some comparisons by increasing by the rle count. We
         * don't need to compare the first byte, hence > 1 instead of
         * > 0 */
        while(len > 1 && buf[pos] == buf[pos + offset])
        {
            int offset1 = ctx->rle_r[pos];
            int offset2 = ctx->rle_r[pos + offset];
            int offset = offset1 < offset2 ? offset1 : offset2;

            LOG(LOG_DEBUG, ("1) compared sucesssfully [%d] %d %d\n",
                            index, pos, pos + offset));

            len -= 1 + offset;
            pos += 1 + offset;
        }
        if(len > 1)
        {
            /* sequence length too short, skip this match */
            continue;
        }

        if(offset < 17)
        {
            /* allocate match struct and add it to matches */
            mp = match_new(ctx, &matches, 1, offset);
        }

        /* Here we know that the current match is atleast as long as
         * the previuos one. let's compare further. */
        len = mp_len;
        pos = index - len;
        while(pos >= 0 && buf[pos] == buf[pos + offset])
        {
            LOG(LOG_DEBUG, ("2) compared sucesssfully [%d] %d %d\n",
                            index, pos, pos + offset));
            ++len;
            --pos;
        }
        if(len > mp_len)
        {
            /* allocate match struct and add it to matches */
            mp = match_new(ctx, &matches, index - pos, offset);
        }
        if(pos < 0)
        {
            /* we have reached the eof, no better matches can be found */
            break;
        }
    }
    LOG(LOG_DEBUG, ("adding matches for index %d to cache\n", index));
    dump_matches(matches);

    return matches;
}

static
int
matchp_keep_this(const_matchp mp)
{
    int val = 1;
    /* if we want to ignore this matchp then return true else false */
    if(mp->len == 1)
    {
        if(mp->offset > 32)
        {
            val = 0;
        }
    }
    return val;
}

static
void
matchp_cache_peek(struct match_ctx *ctx, int pos,
                  const_matchp *litpp, const_matchp *seqpp,
                  matchp lit_tmp, matchp val_tmp)
{
    const_matchp litp, seqp, val;

    seqp = NULL;
    litp = NULL;
    if(pos >= 0)
    {
        val = matches_get(ctx, pos);
        litp = val;
        while(litp->offset != 0)
        {
            litp = litp->next;
        }

        /* inject extra rle match */
        if(ctx->rle_r[pos] > 0)
        {
            val_tmp->offset = 1;
            val_tmp->len = ctx->rle[pos] + 1;
            val_tmp->next = (matchp)val;
            val = val_tmp;
            LOG(LOG_DEBUG, ("injecting rle val(%d,%d)\n",
                            val->len, val->offset));
        }

        while(val != NULL)
        {
            if(val->offset != 0)
            {
                if(matchp_keep_this(val))
                {
                    if(seqp == NULL || val->len > seqp->len ||
                       (val->len == seqp->len && val->offset < seqp->offset))
                    {
                        seqp = val;
                    }
                }
                if(litp->offset == 0 || litp->offset > val->offset)
                {
                    LOG(LOG_DEBUG, ("val(%d,%d)", val->len, val->offset));
                    if(lit_tmp != NULL)
                    {
                        int diff;
                        match tmp2;
                        *tmp2 = *val;
                        tmp2->len = 1;
                        diff = ctx->rle[pos + val->offset];
                        if(tmp2->offset > diff)
                        {
                            tmp2->offset -= diff;
                        }
                        else
                        {
                            tmp2->offset = 1;
                        }
                        LOG(LOG_DEBUG, ("=> litp(%d,%d)",
                                        tmp2->len, tmp2->offset));
                        if(matchp_keep_this(tmp2))
                        {
                            LOG(LOG_DEBUG, (", keeping"));
                            *lit_tmp = *tmp2;
                            litp = lit_tmp;
                        }
                    }
                    LOG(LOG_DEBUG, ("\n"));
                }
            }
            val = val->next;
        }

    }

#if 0
    LOG(LOG_NORMAL, ("[%05d]: ", pos));
    if(litp == NULL)
        LOG(LOG_NORMAL, ("litp(NULL)"));
    else
        LOG(LOG_NORMAL, ("litp(%d,%d)", litp->len, litp->offset));

    if(seqp == NULL)
        LOG(LOG_NORMAL, ("seqp(NULL)"));
    else
        LOG(LOG_NORMAL, ("seqp(%d,%d)", seqp->len, seqp->offset));

    LOG(LOG_NORMAL, ("\n"));
#endif

    if(litpp != NULL) *litpp = litp;
    if(seqpp != NULL) *seqpp = seqp;
}

void matchp_cache_get_enum(match_ctx ctx,       /* IN */
                           matchp_cache_enum mpce)      /* IN/OUT */
{
    mpce->ctx = ctx;
    mpce->pos = ctx->len - 1;
    /*mpce->next = NULL;*/ /* two iterations */
    mpce->next = (void*)mpce; /* just one */
}

const_matchp matchp_cache_enum_get_next(void *matchp_cache_enum)
{
    const_matchp val, lit, seq;
    matchp_cache_enump mpce;

    mpce = matchp_cache_enum;

 restart:
    matchp_cache_peek(mpce->ctx, mpce->pos, &lit, &seq,
                      mpce->tmp1, mpce->tmp2);

    val = lit;
    if(lit == NULL)
    {
        /* the end, reset enum and return NULL */
        mpce->pos = mpce->ctx->len - 1;
        if(mpce->next == NULL)
        {
            mpce->next = (void*)mpce;
            goto restart;
        }
        else
        {
            mpce->next = NULL;
        }
    }
    else
    {
        if(seq != NULL)
        {
            match t1;
            match t2;
            const_matchp next;
            matchp_cache_peek(mpce->ctx, mpce->pos - 1, NULL, &next, t1 ,t2);
            if(next == NULL ||
               (next->len + (mpce->next != NULL && next->len < 3) <= seq->len))
            {
                /* nope, next is not better, use this sequence */
                val = seq;
            }
        }
    }
    if(val != NULL)
    {
        LOG(LOG_DEBUG, ("Using len %05d, offset, %05d\n", val->len, val->offset));
        mpce->pos -= val->len;
    }
    return val;
}
