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

#include <stdlib.h>
#include "log.h"
#include "output.h"

#define OUTPUT_FLAG_REVERSE 1

void output_ctx_init(output_ctx ctx)    /* IN/OUT */
{
    ctx->bitbuf = 1;
    ctx->pos = 0;
    ctx->start = 0;
    ctx->flags = 0;
}

void output_ctx_set_start(output_ctx ctx,       /* IN/OUT */
                          unsigned int pos)     /* IN */
{
    ctx->start = pos;
}

#if 0 /* RH */
void output_ctx_set_reverse(output_ctx ctx)     /* IN/OUT */
{
    ctx->flags |= OUTPUT_FLAG_REVERSE;
}


static void reverse(unsigned char *start, int len)
{
    unsigned char *end = start + len - 1;
    unsigned char tmp;

    while (start < end)
    {
        tmp = *start;
        *start = *end;
        *end = tmp;

        ++start;
        --end;
    }
}

unsigned int output_ctx_close(output_ctx ctx,   /* IN */
                              FILE * out)       /* OUT */
{
    int rval;
    int len;

    rval = 0;
    /* flush the buffer */
    len = ctx->pos - ctx->start;

    if (ctx->start + len > sizeof(ctx->buf))
    {
        LOG(LOG_ERROR, ("error: out of range in output_ctx_close()\n"));
        exit(1);
    }

    if (ctx->flags & OUTPUT_FLAG_REVERSE)
    {
        reverse(ctx->buf + ctx->start, len);
        ctx->flags &= ~OUTPUT_FLAG_REVERSE;
    }

    fwrite(ctx->buf + ctx->start, 1, len, out);
    return len;
}
#endif /* RH */

unsigned int output_get_pos(output_ctx ctx)     /* IN */
{
    return ctx->pos;
}

void output_set_pos(output_ctx ctx,     /* IN */
                    unsigned int pos)   /* IN */
{
    ctx->pos = pos;
}

void output_byte(output_ctx ctx,        /* IN/OUT */
                 unsigned char byte)    /* IN */
{
    /*LOG(LOG_DUMP, ("output_byte: $%02X\n", byte)); */
    ctx->buf[ctx->pos] = byte;
    ++(ctx->pos);
}

void output_copy_bytes(output_ctx ctx,  /* IN */
                       unsigned int src_pos,    /* IN */
                       unsigned int len)        /* IN */
{
    int i;
    if(src_pos > ctx->pos)
    {
        for (i = 0; (unsigned int)i < len; ++i)
        {
            ctx->buf[ctx->pos + i] = ctx->buf[src_pos + i];
        }
    }
    else if(src_pos < ctx->pos)
    {
        for (i = len - 1; i >= 0; --i)
        {
            ctx->buf[ctx->pos + i] = ctx->buf[src_pos + i];
        }
    }
    ctx->pos += len;
}

void output_word(output_ctx ctx,        /* IN/OUT */
                 unsigned short int word)       /* IN */
{
    output_byte(ctx, (unsigned char) (word & 0xff));
    output_byte(ctx, (unsigned char) (word >> 8));
}


void output_bits_flush(output_ctx ctx)  /* IN/OUT */
{
    /* flush the bitbuf including
     * the extra 1 bit acting as eob flag */
    output_byte(ctx, (unsigned char) (ctx->bitbuf & 0xFF));
    if (ctx->bitbuf & 0x100)
    {
        output_byte(ctx, 1);
    }
    /*LOG(LOG_DUMP, ("bitstream flushed 0x%02X\n", ctx->bitbuf & 0xFF)); */

    /* reset it */
    ctx->bitbuf = 1;
}

#if 0 /* RH */
void bits_dump(int count, int val)
{
    static char buf[1024];
    char *pek;
    pek = buf;
    if (count > 0)
    {
        pek += sprintf(pek, "0x%04X, % 2d: ", val, count);
    }
    while (count-- > 0)
    {
        *(pek++) = val & (1 << count) ? '1' : '0';
    }
    *(pek++) = '\0';
    LOG(LOG_NORMAL, ("%s\n", buf));
}
#endif /* RH */

void output_bits(output_ctx ctx,        /* IN/OUT */
                 int count,     /* IN */
                 int val)       /* IN */
{
    /*LOG(LOG_DUMP, ("output_bits: count = %d, val = %d\n", count, val)); */
    /* this makes the bits appear in reversed
     * big endian order in the output stream */
    while (count-- > 0)
    {
        ctx->bitbuf <<= 1;
        ctx->bitbuf |= val & 0x1;
        val >>= 1;
        if (ctx->bitbuf & 0x100)
        {
            /* full byte, flush it */
            output_byte(ctx, (unsigned char) (ctx->bitbuf & 0xFF));
            /*LOG(LOG_DUMP, 
               ("bitstream byte 0x%02X\n", ctx->bitbuf & 0xFF)); */
            ctx->bitbuf = 1;
        }
    }
}

void output_gamma_code(output_ctx ctx,  /* IN/OUT */
                       int code)        /* IN */
{
    output_bits(ctx, 1, 1);
    while (code-- > 0)
    {
        output_bits(ctx, 1, 0);
    }
}
