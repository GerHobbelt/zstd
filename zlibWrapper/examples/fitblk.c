/* fitblk.c contains minimal changes required to be compiled with zlibWrapper:
 * - #include "zlib.h" was changed to #include "zstd_zlibwrapper.h"
 * - writing block to stdout was disabled                          */

/* fitblk.c: example of fitting compressed output to a specified size
   Not copyrighted -- provided to the public domain
   Version 1.1  25 November 2004  Mark Adler */

/* Version history:
   1.0  24 Nov 2004  First version
   1.1  25 Nov 2004  Change deflateInit2() to deflateInit()
                     Use fixed-size, stack-allocated raw buffers
                     Simplify code moving compression to subroutines
                     Use assert() for internal errors
                     Add detailed description of approach
 */

/* Approach to just fitting a requested compressed size:

   fitblk performs three compression passes on a portion of the input
   data in order to determine how much of that input will compress to
   nearly the requested output block size.  The first pass generates
   enough deflate blocks to produce output to fill the requested
   output size plus a specified excess amount (see the EXCESS define
   below).  The last deflate block may go quite a bit past that, but
   is discarded.  The second pass decompresses and recompresses just
   the compressed data that fit in the requested plus excess sized
   buffer.  The deflate process is terminated after that amount of
   input, which is less than the amount consumed on the first pass.
   The last deflate block of the result will be of a comparable size
   to the final product, so that the header for that deflate block and
   the compression ratio for that block will be about the same as in
   the final product.  The third compression pass decompresses the
   result of the second step, but only the compressed data up to the
   requested size minus an amount to allow the compressed stream to
   complete (see the MARGIN define below).  That will result in a
   final compressed stream whose length is less than or equal to the
   requested size.  Assuming sufficient input and a requested size
   greater than a few hundred bytes, the shortfall will typically be
   less than ten bytes.

   If the input is short enough that the first compression completes
   before filling the requested output size, then that compressed
   stream is return with no recompression.

   EXCESS is chosen to be just greater than the shortfall seen in a
   two pass approach similar to the above.  That shortfall is due to
   the last deflate block compressing more efficiently with a smaller
   header on the second pass.  EXCESS is set to be large enough so
   that there is enough uncompressed data for the second pass to fill
   out the requested size, and small enough so that the final deflate
   block of the second pass will be close in size to the final deflate
   block of the third and final pass.  MARGIN is chosen to be just
   large enough to assure that the final compression has enough room
   to complete in all cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "zstd_zlibwrapper.h"

#include "../programs/monolithic_examples.h"


#define LOG_FITBLK(...)   /*printf(__VA_ARGS__)*/
#define local static

/* print nastygram and leave */
local void quit(char *why)
{
    fprintf(stderr, "fitblk abort: %s\n", why);
    exit(1);
}

#define RAWLEN 4096    /* intermediate uncompressed buffer size */

/* compress from file to def until provided buffer is full or end of
   input reached; return last deflate() return value, or Z_ERRNO if
   there was read error on the file */
local int partcompress(FILE *in, zng_streamp def)
{
    int ret, flush;
    unsigned char raw[RAWLEN];

    flush = Z_SYNC_FLUSH;
    do {
        def->avail_in = (uInt)fread(raw, 1, RAWLEN, in);
        if (ferror(in))
            return Z_ERRNO;
        def->next_in = raw;
        if (feof(in))
            flush = Z_FINISH;
        LOG_FITBLK("partcompress1 avail_in=%d total_in=%d avail_out=%d total_out=%d\n", (int)def->avail_in, (int)def->total_in, (int)def->avail_out, (int)def->total_out);
        ret = zng_deflate(def, flush);
        LOG_FITBLK("partcompress2 ret=%d avail_in=%d total_in=%d avail_out=%d total_out=%d\n", ret, (int)def->avail_in, (int)def->total_in, (int)def->avail_out, (int)def->total_out);
        assert(ret != Z_STREAM_ERROR);
    } while (def->avail_out != 0 && flush == Z_SYNC_FLUSH);
    return ret;
}

/* recompress from inf's input to def's output; the input for inf and
   the output for def are set in those structures before calling;
   return last deflate() return value, or Z_MEM_ERROR if inflate()
   was not able to allocate enough memory when it needed to */
local int recompress(zng_streamp inf, zng_streamp def)
{
    int ret, flush;
    unsigned char raw[RAWLEN];

    flush = Z_NO_FLUSH;
    LOG_FITBLK("recompress start\n");
    do {
        /* decompress */
        inf->avail_out = RAWLEN;
        inf->next_out = raw;
        LOG_FITBLK("recompress1inflate avail_in=%d total_in=%d avail_out=%d total_out=%d\n", (int)inf->avail_in, (int)inf->total_in, (int)inf->avail_out, (int)inf->total_out);
        ret = zng_inflate(inf, Z_NO_FLUSH);
        LOG_FITBLK("recompress2inflate avail_in=%d total_in=%d avail_out=%d total_out=%d\n", (int)inf->avail_in, (int)inf->total_in, (int)inf->avail_out, (int)inf->total_out);
        assert(ret != Z_STREAM_ERROR && ret != Z_DATA_ERROR &&
               ret != Z_NEED_DICT);
        if (ret == Z_MEM_ERROR)
            return ret;

        /* compress what was decompressed until done or no room */
        def->avail_in = RAWLEN - inf->avail_out;
        def->next_in = raw;
        if (inf->avail_out != 0)
            flush = Z_FINISH;
        LOG_FITBLK("recompress1deflate avail_in=%d total_in=%d avail_out=%d total_out=%d\n", (int)def->avail_in, (int)def->total_in, (int)def->avail_out, (int)def->total_out);
        ret = zng_deflate(def, flush);
        LOG_FITBLK("recompress2deflate ret=%d avail_in=%d total_in=%d avail_out=%d total_out=%d\n", ret, (int)def->avail_in, (int)def->total_in, (int)def->avail_out, (int)def->total_out);
        assert(ret != Z_STREAM_ERROR);
    } while (ret != Z_STREAM_END && def->avail_out != 0);
    return ret;
}

#define EXCESS 256      /* empirically determined stream overage */
#define MARGIN 8        /* amount to back off for completion */

/* compress from stdin to fixed-size block on stdout */

#if defined(BUILD_MONOLITHIC)
#define main(cnt, arr)      zstd_fitblk_example_main(cnt, arr)
#endif

int main(int argc, const char** argv)
{
    int ret;                /* return code */
    unsigned size;          /* requested fixed output block size */
    unsigned have;          /* bytes written by deflate() call */
    unsigned char *blk;     /* intermediate and final stream */
    unsigned char *tmp;     /* close to desired size stream */
    zng_stream def, inf;      /* zlib deflate and inflate states */
	char* argend;

    /* get requested output size */
    if (argc != 2)
        quit("need one argument: size of output block");
    ret = (int)strtol(argv[1], &argend, 10);
    if (argend[0] != 0)
        quit("argument must be a number");
    if (ret < 8)            /* 8 is minimum zlib stream size */
        quit("need positive size of 8 or greater");
    size = (unsigned)ret;

    printf("zlib version %s\n", ZLIBNG_VERSION);
    if (ZWRAP_isUsingZSTDcompression()) printf("zstd version %s\n", zstdVersion());

    /* allocate memory for buffers and compression engine */
    blk = (unsigned char*)malloc(size + EXCESS);
    def.zalloc = Z_NULL;
    def.zfree = Z_NULL;
    def.opaque = Z_NULL;
    ret = zng_deflateInit(&def, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK || blk == NULL)
        quit("out of memory");

    /* compress from stdin until output full, or no more input */
    def.avail_out = size + EXCESS;
    def.next_out = blk;
    LOG_FITBLK("partcompress1 total_in=%d total_out=%d\n", (int)def.total_in, (int)def.total_out);
    ret = partcompress(stdin, &def);
    printf("partcompress total_in=%d total_out=%d\n", (int)def.total_in, (int)def.total_out);
    if (ret == Z_ERRNO)
        quit("error reading input");

    /* if it all fit, then size was undersubscribed -- done! */
    if (ret == Z_STREAM_END && def.avail_out >= EXCESS) {
        /* write block to stdout */
        have = size + EXCESS - def.avail_out;
        if (fwrite(blk, 1, have, stdout) != have || ferror(stdout))
            quit("error writing output");

        /* clean up and print results to stderr */
        ret = zng_deflateEnd(&def);
        assert(ret != Z_STREAM_ERROR);
        free(blk);
        fprintf(stderr,
                "%u bytes unused out of %u requested (all input)\n",
                size - have, size);
        return 0;
    }

    /* it didn't all fit -- set up for recompression */
    inf.zalloc = Z_NULL;
    inf.zfree = Z_NULL;
    inf.opaque = Z_NULL;
    inf.avail_in = 0;
    inf.next_in = Z_NULL;
    ret = zng_inflateInit(&inf);
    tmp = (unsigned char*)malloc(size + EXCESS);
    if (ret != Z_OK || tmp == NULL)
        quit("out of memory");
    ret = zng_deflateReset(&def);
    assert(ret != Z_STREAM_ERROR);

    /* do first recompression close to the right amount */
    inf.avail_in = size + EXCESS;
    inf.next_in = blk;
    def.avail_out = size + EXCESS;
    def.next_out = tmp;
    LOG_FITBLK("recompress1 inf.total_in=%d def.total_out=%d\n", (int)inf.total_in, (int)def.total_out);
    ret = recompress(&inf, &def);
    LOG_FITBLK("recompress1 inf.total_in=%d def.total_out=%d\n", (int)inf.total_in, (int)def.total_out);
    if (ret == Z_MEM_ERROR)
        quit("out of memory");

    /* set up for next recompression */
    ret = zng_inflateReset(&inf);
    assert(ret != Z_STREAM_ERROR);
    ret = zng_deflateReset(&def);
    assert(ret != Z_STREAM_ERROR);

    /* do second and final recompression (third compression) */
    inf.avail_in = size - MARGIN;   /* assure stream will complete */
    inf.next_in = tmp;
    def.avail_out = size;
    def.next_out = blk;
    LOG_FITBLK("recompress2 inf.total_in=%d def.total_out=%d\n", (int)inf.total_in, (int)def.total_out);
    ret = recompress(&inf, &def);
    LOG_FITBLK("recompress2 inf.total_in=%d def.total_out=%d\n", (int)inf.total_in, (int)def.total_out);
    if (ret == Z_MEM_ERROR)
        quit("out of memory");
    assert(ret == Z_STREAM_END);    /* otherwise MARGIN too small */

    /* done -- write block to stdout */
    have = size - def.avail_out;
    if (fwrite(blk, 1, have, stdout) != have || ferror(stdout))
        quit("error writing output");

    /* clean up and print results to stderr */
    free(tmp);
    ret = zng_inflateEnd(&inf);
    assert(ret != Z_STREAM_ERROR);
    ret = zng_deflateEnd(&def);
    assert(ret != Z_STREAM_ERROR);
    free(blk);
    fprintf(stderr,
            "%u bytes unused out of %u requested (%lu input)\n",
            size - have, size, def.total_in);
    return 0;
}
