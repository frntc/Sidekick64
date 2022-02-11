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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "log.h"
#include "search.h"
#include "radix.h"
#include "chunkpool.h"

#define DOUBLE_OFFSET_TABLES

struct _interval_node {
    int start;
    int score;
    struct _interval_node *next;
    signed char prefix;  /* RH: signed */
    signed char bits;  /* RH: signed */
    signed char depth;  /* RH: signed */
    signed char flags;  /* RH: signed */
};

typedef struct _interval_node interval_node[1];
typedef struct _interval_node *interval_nodep;

static
void
interval_node_init(interval_nodep inp, int start, int depth, int flags)
{
    inp->start = start;
    inp->flags = flags;
    inp->depth = depth;
    inp->bits = 0;
    inp->prefix = flags >= 0 ? flags : depth + 1;
    inp->score = -1;
    inp->next = NULL;
}

static
interval_nodep interval_node_clone(interval_nodep inp)
{
    interval_nodep inp2 = NULL;

    if(inp != NULL)
    {
	inp2 = malloc(sizeof(interval_node));
	if (inp2 == NULL)
	{
	    LOG(LOG_ERROR, ("out of memory error in file %s, line %d\n",
			    __FILE__, __LINE__));
	    //exit(0);
        return;
	}
	/* copy contents */
	*inp2 = *inp;
	inp2->next = interval_node_clone(inp->next);
    }

    return inp2;
}

static
void interval_node_delete(interval_nodep inp)
{
    while (inp != NULL)
    {
        interval_nodep inp2 = inp;
        inp = inp->next;
        free(inp2);
    }
}

#if 0 /* RH */
static
void interval_node_dump(interval_nodep inp)
{
    int end;

    end = 0;
    while (inp != NULL)
    {
        end = inp->start + (1 << inp->bits);
        LOG(LOG_NORMAL, ("%X", inp->bits));
        /*LOG(LOG_NORMAL, ("%d[1�%d], ", inp->start, inp->bits));*/
        inp = inp->next;
    }
    LOG(LOG_NORMAL, ("[eol@%d]\n", end));
}
#endif /* RH */

float optimal_encode_int(int arg, void *priv, output_ctxp out)
{
    interval_nodep inp;
    int end;

    float val;

    inp = (interval_nodep) priv;
    val = 1000000.0;
    end = 0;
    while (inp != NULL)
    {
        end = inp->start + (1 << inp->bits);
        if (arg >= inp->start && arg < end)
        {
            break;
        }
        inp = inp->next;
    }
    if (inp != NULL)
    {
        val = (float) (inp->prefix + inp->bits);
    }
    /*LOG(LOG_DUMP, ("encoding %d to %0.1f bits\n", arg, val)); */
    if (out)
    {
        output_bits(out, inp->bits, arg - inp->start);
        if (inp->flags < 0)
        {
            /*LOG(LOG_DUMP, ("gamma prefix code = %d", inp->depth)); */
            output_gamma_code(out, inp->depth);
        } else
        {
            /*LOG(LOG_DUMP, ("flat prefix %d bits ", inp->depth)); */
            output_bits(out, inp->prefix, inp->depth);
        }
    }

    return val;
}

float optimal_encode(const_matchp mp, encode_match_data emd)
{
    interval_nodep *offset;
    float bits;
    encode_match_privp data;

    data = emd->priv;
    offset = data->offset_f_priv;
    /*LOG(LOG_DUMP, ("judging offset %d len %d", offset, len)); */

    bits = 0.0;
    if (mp->offset == 0)
    {
        bits += 9.0f * mp->len;
        data->lit_num += mp->len;
        data->lit_bits += bits;
    } else
    {
        bits += 1.0;
        switch (mp->len)
        {
        case 0:
            LOG(LOG_ERROR, ("bad len\n"));
    //        exit(1);
            return;
            break;
        case 1:
#if 1
            bits += data->offset_f(mp->offset, offset[0], emd->out);
#else
            bits += 4.0;
            if (mp->offset > (1 << 4))
            {
                bits += 1000000.0;
            }
            if (emd->out)
            {
                output_bits(emd->out, 4, mp->offset - 1);
            }
#endif
            break;
#ifdef DOUBLE_OFFSET_TABLES
        case 2:
            bits += data->offset_f(mp->offset, offset[1], emd->out);
            break;
#if 0
        case 3:
        case 4:
            bits += data->offset_f(mp->offset, offset[2], emd->out);
            break;
#endif
#endif
        default:
            bits += data->offset_f(mp->offset, offset[7], emd->out);
            break;
        }
        bits += data->len_f(mp->len, data->len_f_priv, emd->out);
        if (bits > (9.0 * mp->len))
        {
            /* lets make literals out of it */
            data->lit_num += 1;
            data->lit_bits += bits;
        } else
        {
            if (mp->offset == 1)
            {
                data->rle_num += 1;
                data->rle_bits += bits;
            } else
            {
                data->seq_num += 1;
                data->seq_bits += bits;
            }
        }
    }
    return bits;
}

struct _optimize_arg {
    radix_root cache;
    int *stats;
    int *stats2;
    int max_depth;
    int flags;
    struct chunkpool in_pool[1];
};

#define CACHE_KEY(START, DEPTH, MAXDEPTH) ((int)((START)*(MAXDEPTH)|DEPTH))

typedef struct _optimize_arg optimize_arg[1];
typedef struct _optimize_arg optimize_argp;

static interval_nodep
optimize1(optimize_arg arg, int start, int depth)
{
    interval_node inp;
    interval_nodep best_inp;
    int end, i;
    int start_count, end_count;

    /*LOG(LOG_DUMP, ("IN start %d, depth %d\n", start, depth)); */

    do
    {
        best_inp = NULL;
        if (arg->stats[start] == 0)
        {
            break;
        }
        int key = CACHE_KEY(start, depth, arg->max_depth);
        best_inp = radix_node_get(arg->cache, key);
        if (best_inp != NULL)
        {
            break;
        }

        interval_node_init(inp, start, depth, arg->flags);

        for (i = 0; i < 16; ++i)
        {
            inp->next = NULL;
            inp->bits = i;
            end = start + (1 << i);

            start_count = end_count = 0;
            if (start < 65536)
            {
                start_count = arg->stats[start];
                if (end < 65536)
                {
                    end_count = arg->stats[end];
                }
            }

            inp->score = (start_count - end_count) *
                (inp->prefix + inp->bits);

            /* one index below */
            /*LOG(LOG_DUMP, ("interval score: [%d�%d[%d\n",
               start, i, inp->score)); */
            if (end_count > 0)
            {
                int penalty;
                /* we're not done, now choose between using
                 * more bits, go deeper or skip the rest */
                if (depth + 1 < arg->max_depth)
                {
                    /* we can go deeper, let's try that */
                    inp->next = optimize1(arg, end, depth + 1);
                }
                /* get the penalty for skipping */
                penalty = 1000000;
                if (arg->stats2 != NULL)
                {
                    penalty = arg->stats2[end];
                }
                if (inp->next != NULL && inp->next->score < penalty)
                {
                    penalty = inp->next->score;
                }
                inp->score += penalty;
            }
            if (best_inp == NULL || inp->score < best_inp->score)
            {
                /* it's the new best in town, use it */
                if (best_inp == NULL)
                {
                    /* allocate if null */
                    best_inp = chunkpool_malloc(arg->in_pool);
                }
                *best_inp = *inp;
            }
        }
        if (best_inp != NULL)
        {
            radix_node_set(arg->cache, key, best_inp);
        }
    }
    while (0);
    /*LOG(LOG_DUMP, ("OUT depth %d: ", depth)); */
    /*interval_node_dump(best_inp); */
    return best_inp;
}

static interval_nodep
optimize(int stats[65536], int stats2[65536], int max_depth, int flags)
{
    optimize_arg arg;

    interval_nodep inp;

    arg->stats = stats;
    arg->stats2 = stats2;

    arg->max_depth = max_depth;
    arg->flags = flags;

    chunkpool_init(arg->in_pool, sizeof(interval_node));

    radix_tree_init(arg->cache);

    inp = optimize1(arg, 1, 0);

    /* use normal malloc for the winner */
    inp = interval_node_clone(inp);

    /* cleanup */
    radix_tree_free(arg->cache, NULL, NULL);
    chunkpool_free(arg->in_pool);

    return inp;
}


/*static interval_nodep optimal_offset[16] = {NULL, NULL};
  static interval_nodep optimal_len = NULL;*/

void optimal_init(encode_match_data emd)        /* OUT */
{
    encode_match_privp data;
    interval_nodep *inpp;

    data = emd->priv;

    memset(data, 0, sizeof(encode_match_priv));

    data->offset_f = optimal_encode_int;
    data->len_f = optimal_encode_int;
    inpp = malloc(sizeof(interval_nodep[8]));
    inpp[0] = NULL;
    inpp[1] = NULL;
    inpp[2] = NULL;
    inpp[3] = NULL;
    inpp[4] = NULL;
    inpp[5] = NULL;
    inpp[6] = NULL;
    inpp[7] = NULL;
    data->offset_f_priv = inpp;
    data->len_f_priv = NULL;
}

void optimal_free(encode_match_data emd)        /* IN */
{
    encode_match_privp data;
    interval_nodep *inpp;
    interval_nodep inp;

    data = emd->priv;

    inpp = data->offset_f_priv;
    if (inpp != NULL)
    {
        interval_node_delete(inpp[0]);
        interval_node_delete(inpp[1]);
        interval_node_delete(inpp[2]);
        interval_node_delete(inpp[3]);
        interval_node_delete(inpp[4]);
        interval_node_delete(inpp[5]);
        interval_node_delete(inpp[6]);
        interval_node_delete(inpp[7]);
    }
    free(inpp);

    inp = data->len_f_priv;
    interval_node_delete(inp);

    data->offset_f_priv = NULL;
    data->len_f_priv = NULL;
}

#if 0 /* RH */
void freq_stats_dump(int arr[65536])
{
    int i;
    for (i = 0; i < 32; ++i)
    {
        LOG(LOG_NORMAL, ("%d, ", arr[i] - arr[i + 1]));
    }
    LOG(LOG_NORMAL, ("\n"));
}

void freq_stats_dump_raw(int arr[65536])
{
    int i;
    for (i = 0; i < 32; ++i)
    {
        LOG(LOG_NORMAL, ("%d, ", arr[i]));
    }
    LOG(LOG_NORMAL, ("\n"));
}
#endif /* RH */

void optimal_optimize(encode_match_data emd,    /* IN/OUT */
                      matchp_enum_get_next_f * f,       /* IN */
                      void *matchp_enum)        /* IN */
{
    encode_match_privp data;
    const_matchp mp;
    interval_nodep *offset;
    static int offset_arr[8][65536];
    static int offset_parr[8][65536];
    static int len_arr[65536];
    int treshold;

    int i, j;
    void *priv1;

    data = emd->priv;

    memset(offset_arr, 0, sizeof(offset_arr));
    memset(offset_parr, 0, sizeof(offset_parr));
    memset(len_arr, 0, sizeof(len_arr));

    offset = data->offset_f_priv;

    /* first the lens */
    priv1 = matchp_enum;
    while ((mp = f(priv1)) != NULL && mp->len > 0)
    {
        if (mp->offset > 0)
        {
            len_arr[mp->len] += 1;
        }
    }

    for (i = 65534; i >= 0; --i)
    {
        len_arr[i] += len_arr[i + 1];
    }

    data->len_f_priv = optimize(len_arr, NULL, 16, -1);

    /* then the offsets */
    priv1 = matchp_enum;
    while ((mp = f(priv1)) != NULL && mp->len > 0)
    {
        if (mp->offset > 0)
        {
            treshold = mp->len * 9;
            treshold -= 1 + (int) optimal_encode_int(mp->len,
                                                     data->len_f_priv,
                                                     NULL);
            switch (mp->len)
            {
            case 0:
                LOG(LOG_NORMAL, ("bad len\n"));
                //exit(0);
                return;
                break;
            case 1:
#if 1
                offset_parr[0][mp->offset] += treshold;
                offset_arr[0][mp->offset] += 1;
#endif
                break;
#ifdef DOUBLE_OFFSET_TABLES
            case 2:
                offset_parr[1][mp->offset] += treshold;
                offset_arr[1][mp->offset] += 1;
                break;
#if 0
            case 3:
            case 4:
                offset_parr[2][mp->offset] += treshold;
                offset_arr[2][mp->offset] += 1;
                break;
#endif
#endif
            default:
                offset_parr[7][mp->offset] += treshold;
                offset_arr[7][mp->offset] += 1;
                break;
            }
        }
    }

    for (i = 65534; i >= 0; --i)
    {
        for (j = 0; j < 8; ++j)
        {
            offset_arr[j][i] += offset_arr[j][i + 1];
            offset_parr[j][i] += offset_parr[j][i + 1];
        }
    }

    offset[0] = optimize(offset_arr[0], offset_parr[0], 1 << 2, 2);
    offset[1] = optimize(offset_arr[1], offset_parr[1], 1 << 4, 4);
    offset[2] = optimize(offset_arr[2], offset_parr[2], 1 << 4, 4);
    offset[3] = optimize(offset_arr[3], offset_parr[3], 1 << 4, 4);
    offset[4] = optimize(offset_arr[4], offset_parr[4], 1 << 4, 4);
    offset[5] = optimize(offset_arr[5], offset_parr[5], 1 << 4, 4);
    offset[6] = optimize(offset_arr[6], offset_parr[6], 1 << 4, 4);
    offset[7] = optimize(offset_arr[7], offset_parr[7], 1 << 4, 4);
}

#if 0 /* RH */
static int optimal_fixup1(interval_nodep *npp,
                          int start, int depth, int flags, int max)
{
    int size = max - start;
    if (*npp == NULL)
    {
        int bits = 0;
        LOG(LOG_DEBUG, ("max %d, size %d\n", max, size));
        if(depth < 15)
        {
            interval_node in;
            while (size > 0)
            {
                size >>= 1;
                ++bits;
            }
            in->start = start;
            in->depth = depth;
            in->flags = flags;
            in->bits = bits > 15 ? 15 : bits;
            in->next = NULL;

            *npp = interval_node_clone(in);
        }
    }
    if(*npp != NULL)
    {
        int len = (1 << (*npp)->bits);
        size = optimal_fixup1(&((*npp)->next),
                              start + len, depth + 1, flags, max);
        if(size > 0)
        {
            int bits = (*npp)->bits;
            int diff = 32768 - (1 << bits);
            if(diff > 0)
            {
                interval_nodep np;
                (*npp)->bits = 15;

                np = (*npp)->next;
                while(np != NULL)
                {
                    np->start += diff;
                    np = np->next;
                }
                size -= diff;
            }
        }
    }
    return size;
}

void optimal_fixup(encode_match_data emd, int max_len, int max_offset)
{
    encode_match_privp data;
    interval_nodep *offset;
    interval_nodep len;

    data = emd->priv;

    offset = data->offset_f_priv;
    len = data->len_f_priv;

    optimal_fixup1(&len, 1, 0, -1, max_len);

    /*optimal_fixup1(&(offset[7]), 1, 0, -4, max_offset);*/
}

void optimal_dump(encode_match_data emd)
{
    encode_match_privp data;
    interval_nodep *offset;
    interval_nodep len;

    data = emd->priv;

    offset = data->offset_f_priv;
    len = data->len_f_priv;

    LOG(LOG_NORMAL, ("lens:             "));
    interval_node_dump(len);

    LOG(LOG_NORMAL, ("offsets (len =1): "));
    interval_node_dump(offset[0]);

    LOG(LOG_NORMAL, ("offsets (len =2): "));
    interval_node_dump(offset[1]);
#if 0
    LOG(LOG_NORMAL, ("offsets (len =3): "));
    interval_node_dump(offset[2]);

    LOG(LOG_NORMAL, ("offsets (len =4): "));
    interval_node_dump(offset[3]);

    LOG(LOG_NORMAL, ("offsets (len =5): "));
    interval_node_dump(offset[4]);

    LOG(LOG_NORMAL, ("offsets (len =6): "));
    interval_node_dump(offset[5]);

    LOG(LOG_NORMAL, ("offsets (len =7): "));
    interval_node_dump(offset[6]);
#endif
    LOG(LOG_NORMAL, ("offsets (len =8): "));
    interval_node_dump(offset[7]);
}
#endif /* RH */

static
void interval_out(output_ctx out, interval_nodep inp1, int size)
{
    unsigned char buffer[256];
    unsigned char count;
    interval_nodep inp;

    count = 0;

    memset(buffer, 15, sizeof(buffer));
    inp = inp1;
    while (inp != NULL)
    {
        ++count;
        /*LOG(LOG_DUMP, ("bits %d, lo %d, hi %d\n",
           inp->bits, inp->start & 0xFF, inp->start >> 8)); */
        buffer[sizeof(buffer) - count] = inp->bits;
        inp = inp->next;
    }

    while (size > 0)
    {
        int b;
        b = buffer[sizeof(buffer) - size];
        /*LOG(LOG_DUMP, ("outputting nibble %d\n", b)); */
        output_bits(out, 4, b);
        size--;
    }
}

void optimal_out(output_ctx out,        /* IN/OUT */
                 encode_match_data emd) /* IN */
{
    encode_match_privp data;
    interval_nodep *offset;
    interval_nodep len;

    data = emd->priv;

    offset = data->offset_f_priv;
    len = data->len_f_priv;

    interval_out(out, offset[0], 4);
    interval_out(out, offset[1], 16);
    interval_out(out, offset[7], 16);
    interval_out(out, len, 16);
}
