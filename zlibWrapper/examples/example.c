/* example.c contains minimal changes required to be compiled with zlibWrapper:
 * - #include "zlib.h" was changed to #include "zstd_zlibwrapper.h"
 * - test_flush() and test_sync() use functions not supported by zlibWrapper
     therefore they are disabled while zstd compression is turned on     */

/* example.c -- usage example of the zlib compression library
 * Copyright (C) 1995-2006, 2011, 2016 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zstd_zlibwrapper.h"
#include "deflate.h"

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>

#define TESTFILE "foo.gz"

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

static const char hello[] = "hello, hello!";
/* "hello world" would be more standard, but the repeated "hello"
 * stresses the compression code better, sorry...
 */

static const char dictionary[] = "hello";
static unsigned long dictId = 0; /* Adler32 value of the dictionary */


static void test_compress      (unsigned char *compr, z_size_t comprLen,unsigned char *uncompr, z_size_t uncomprLen);
static void test_gzio          (const char *fname, unsigned char *uncompr, z_size_t uncomprLen);
static void test_deflate       (unsigned char *compr, size_t comprLen);
static void test_inflate       (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
static void test_large_deflate (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, int zng_params);
static void test_large_inflate (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
static void test_flush         (unsigned char *compr, z_size_t *comprLen);
static void test_sync          (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
static void test_dict_deflate  (unsigned char *compr, size_t comprLen);
static void test_dict_inflate  (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);

#if defined(BUILD_MONOLITHIC)
#define main(cnt, arr)      zstd_zlibWrapper_main(cnt, arr)
#endif

int  main               (int argc, const char** argv);


static alloc_func zalloc = NULL;
static free_func zfree = NULL;

/* ===========================================================================
 * Test compress() and uncompress()
 */
static void test_compress(unsigned char *compr, z_size_t comprLen, unsigned char *uncompr, z_size_t uncomprLen) {
    int err;
    size_t len = strlen(hello)+1;

    err = PREFIX(compress)(compr, &comprLen, (const unsigned char*)hello, (z_size_t)len);
    CHECK_ERR(err, "compress");

    strcpy((char*)uncompr, "garbage");

    err = PREFIX(uncompress)(uncompr, &uncomprLen, compr, comprLen);
    CHECK_ERR(err, "uncompress");

    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad uncompress\n");
        exit(1);
    } else {
        printf("uncompress(): %s\n", (char *)uncompr);
    }
}

/* ===========================================================================
 * Test read/write of .gz files
 */
static void test_gzio(const char *fname, unsigned char *uncompr, z_size_t uncomprLen) {
#if defined(NO_GZCOMPRESS) || !defined(WITH_GZFILEOP)
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
#else
    int err;
    size_t read;
    size_t len = strlen(hello)+1;
    gzFile file;
    z_off64_t pos;
    z_off64_t comprLen;

    /* Write gz file with test data */
    file = PREFIX(gzopen)(fname, "wb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        exit(1);
    }
    /* Write hello, hello! using gzputs and gzprintf */
    PREFIX(gzputc)(file, 'h');
    if (PREFIX(gzputs)(file, "ello") != 4) {
        fprintf(stderr, "gzputs err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    if (PREFIX(gzprintf)(file, ", %s!", "hello") != 8) {
        fprintf(stderr, "gzprintf err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    /* Write string null-teriminator using gzseek */
    if (PREFIX(gzseek)(file, 1L, SEEK_CUR) < 0)
    {
        fprintf(stderr, "gzseek error, gztell=%ld\n", (long)PREFIX(gztell)(file));
        exit(1);
    }
    /* Write hello, hello! using gzfwrite using best compression level */
    if (PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY) != Z_OK) {
        fprintf(stderr, "gzsetparams err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    if (PREFIX(gzfwrite)(hello, len, 1, file) == 0) {
        fprintf(stderr, "gzfwrite err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    /* Flush compressed bytes to file */
    if (PREFIX(gzflush)(file, Z_SYNC_FLUSH) != Z_OK) {
        fprintf(stderr, "gzflush err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    comprLen = PREFIX(gzoffset)(file);
    if (comprLen <= 0) {
        fprintf(stderr, "gzoffset err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    PREFIX(gzclose)(file);

    /* Open gz file we previously wrote */
    file = PREFIX(gzopen)(fname, "rb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        exit(1);
    }
    /* Read uncompressed data - hello, hello! string twice */
    strcpy((char*)uncompr, "garbages");
    if (PREFIX(gzread)(file, uncompr, (unsigned)uncomprLen) != (int)(len + len)) {
        fprintf(stderr, "gzread err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad gzread: %s\n", (char*)uncompr);
        exit(1);
    } else {
        printf("gzread(): %s\n", (char*)uncompr);
    }
    /* Check position at the end of the gz file */
    if (PREFIX(gzeof)(file) != 1) {
        fprintf(stderr, "gzeof err: not reporting end of stream\n");
        exit(1);
    }
    /* Seek backwards mid-string and check char reading with gzgetc and gzungetc */
    pos = PREFIX(gzseek)(file, -22L, SEEK_CUR);
    if (pos != 6 || PREFIX(gztell)(file) != pos) {
        fprintf(stderr, "gzseek error, pos=%ld, gztell=%ld\n",
                (long)pos, (long)PREFIX(gztell)(file));
        exit(1);
    }
    if (PREFIX(gzgetc)(file) != ' ') {
        fprintf(stderr, "gzgetc error\n");
        exit(1);
    }
    if (PREFIX(gzungetc)(' ', file) != ' ') {
        fprintf(stderr, "gzungetc error\n");
        exit(1);
    }
    /* Read first hello, hello! string with gzgets */
    strcpy((char*)uncompr, "garbages");
    PREFIX(gzgets)(file, (char*)uncompr, (int)uncomprLen);
    if (strlen((char*)uncompr) != 7) { /* " hello!" */
        fprintf(stderr, "gzgets err after gzseek: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    if (strcmp((char*)uncompr, hello + 6)) {
        fprintf(stderr, "bad gzgets after gzseek\n");
        exit(1);
    } else {
        printf("gzgets() after gzseek: %s\n", (char*)uncompr);
    }
    /* Seek to second hello, hello! string */
    pos = PREFIX(gzseek)(file, 14L, SEEK_SET);
    if (pos != 14 || PREFIX(gztell)(file) != pos) {
        fprintf(stderr, "gzseek error, pos=%ld, gztell=%ld\n",
                (long)pos, (long)PREFIX(gztell)(file));
        exit(1);
    }
    /* Check position not at end of file */
    if (PREFIX(gzeof)(file) != 0) {
        fprintf(stderr, "gzeof err: reporting end of stream\n");
        exit(1);
    }
    /* Read first hello, hello! string with gzfread */
    strcpy((char*)uncompr, "garbages");
    read = PREFIX(gzfread)(uncompr, uncomprLen, 1, file);
    if (strcmp((const char *)uncompr, hello) != 0) {
        fprintf(stderr, "bad gzgets\n");
        exit(1);
    } else {
        printf("gzgets(): %s\n", (char*)uncompr);
    }
    pos = PREFIX(gzoffset)(file);
    if (pos < 0 || pos != (comprLen + 10)) {
        fprintf(stderr, "gzoffset err: wrong offset at end\n");
        exit(1);
    }
    /* Trigger an error and clear it with gzclearerr */
    PREFIX(gzfread)(uncompr, (size_t)-1, (size_t)-1, file);
    PREFIX(gzerror)(file, &err);
    if (err == 0) {
        fprintf(stderr, "gzerror err: no error returned\n");
        exit(1);
    }
    PREFIX(gzclearerr)(file);
    PREFIX(gzerror)(file, &err);
    if (err != 0) {
        fprintf(stderr, "gzclearerr err: not zero %d\n", err);
        exit(1);
    }

    PREFIX(gzclose)(file);

    if (PREFIX(gzclose)(NULL) != Z_STREAM_ERROR) {
        fprintf(stderr, "gzclose unexpected return when handle null\n");
        exit(1);
    }
    (void)read;
#endif
}

/* ===========================================================================
 * Test deflate() with small buffers
 */
static void test_deflate(unsigned char *compr, size_t comprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;
    c_stream.total_in = 0;
    c_stream.total_out = 0;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }
    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with small buffers
 */
static void test_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen) {
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = 0;
    d_stream.next_out = uncompr;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad inflate\n");
        exit(1);
    } else {
        printf("inflate(): %s\n", (char *)uncompr);
    }
}

static unsigned int diff;

/* ===========================================================================
 * Test deflate() with large buffers and dynamic change of compression level
 */
static void test_large_deflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, int zng_params) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
#ifndef ZLIB_COMPAT
    int level = -1;
    int strategy = -1;
    zng_deflate_param_value params[2];

    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &level;
    params[0].size = sizeof(level);

    params[1].param = Z_DEFLATE_STRATEGY;
    params[1].buf = &strategy;
    params[1].size = sizeof(strategy);
#endif

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_SPEED);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_out = compr;
    c_stream.avail_out = (unsigned int)comprLen;

    /* At this point, uncompr is still mostly zeroes, so it should compress
     * very well:
     */
    c_stream.next_in = uncompr;
    c_stream.avail_in = (unsigned int)uncomprLen;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");
    if (c_stream.avail_in != 0) {
        fprintf(stderr, "deflate not greedy\n");
        exit(1);
    }

    /* Feed in already compressed data and switch to no compression: */
    if (zng_params) {
#ifndef ZLIB_COMPAT
        zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        if (level != Z_BEST_SPEED) {
            fprintf(stderr, "Expected compression level Z_BEST_SPEED, got %d\n", level);
            exit(1);
        }
        if (strategy != Z_DEFAULT_STRATEGY) {
            fprintf(stderr, "Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
            exit(1);
        }
        level = Z_NO_COMPRESSION;
        strategy = Z_DEFAULT_STRATEGY;
        zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
        fprintf(stderr, "test_large_deflate() called with zng_params=1 in compat mode\n");
        exit(1);
#endif
    } else {
        PREFIX(deflateParams)(&c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
    }
    c_stream.next_in = compr;
    diff = (unsigned int)(c_stream.next_out - compr);
    c_stream.avail_in = diff;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");

    /* Switch back to compressing mode: */
    if (zng_params) {
#ifndef ZLIB_COMPAT
        level = -1;
        strategy = -1;
        zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        if (level != Z_NO_COMPRESSION) {
            fprintf(stderr, "Expected compression level Z_NO_COMPRESSION, got %d\n", level);
            exit(1);
        }
        if (strategy != Z_DEFAULT_STRATEGY) {
            fprintf(stderr, "Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
            exit(1);
        }
        level = Z_BEST_COMPRESSION;
        strategy = Z_FILTERED;
        zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
        fprintf(stderr, "test_large_deflate() called with zng_params=1 in compat mode\n");
        exit(1);
#endif
    } else {
        PREFIX(deflateParams)(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
    }
    c_stream.next_in = uncompr;
    c_stream.avail_in = (unsigned int)uncomprLen;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate should report Z_STREAM_END\n");
        exit(1);
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with large buffers
 */
static void test_large_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen) {
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)comprLen;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    for (;;) {
        d_stream.next_out = uncompr;            /* discard the output */
        d_stream.avail_out = (unsigned int)uncomprLen;
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "large inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (d_stream.total_out != 2*uncomprLen + diff) {
        fprintf(stderr, "bad large inflate: %" PRIu64 "\n", (uint64_t)d_stream.total_out);
        exit(1);
    } else {
        printf("large_inflate(): OK\n");
    }
}

/* ===========================================================================
 * Test deflate() with full flush
 */
static void test_flush(unsigned char *compr, z_size_t *comprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    unsigned int len = (unsigned int)strlen(hello)+1;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;
    c_stream.avail_in = 3;
    c_stream.avail_out = (unsigned int)*comprLen;
    err = PREFIX(deflate)(&c_stream, Z_FULL_FLUSH);
    CHECK_ERR(err, "deflate");

    compr[3]++; /* force an error in first compressed block */
    c_stream.avail_in = len - 3;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        CHECK_ERR(err, "deflate");
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    *comprLen = (z_size_t)c_stream.total_out;
}

/* ===========================================================================
 * Test inflateSync()
 */
static void test_sync(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen) {
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = 2; /* just read the zlib header */

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    d_stream.next_out = uncompr;
    d_stream.avail_out = (unsigned int)uncomprLen;

    err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "inflate");

    d_stream.avail_in = (unsigned int)comprLen-2;   /* read all compressed data */
    err = PREFIX(inflateSync)(&d_stream);           /* but skip the damaged part */
    CHECK_ERR(err, "inflateSync");

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        fprintf(stderr, "inflate should report Z_STREAM_END\n");
        exit(1);
    }
    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    printf("after inflateSync(): hel%s\n", (char *)uncompr);
}

/* ===========================================================================
 * Test deflate() with preset dictionary
 */
static void test_dict_deflate(unsigned char *compr, size_t comprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;
    c_stream.adler = 0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    err = PREFIX(deflateSetDictionary)(&c_stream,
                (const unsigned char*)dictionary, (int)sizeof(dictionary));
    CHECK_ERR(err, "deflateSetDictionary");

    dictId = c_stream.adler;
    c_stream.next_out = compr;
    c_stream.avail_out = (unsigned int)comprLen;

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (unsigned int)strlen(hello)+1;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate should report Z_STREAM_END\n");
        exit(1);
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with a preset dictionary
 */
static void test_dict_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen) {
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage garbage garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.adler = 0;
    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)comprLen;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    d_stream.next_out = uncompr;
    d_stream.avail_out = (unsigned int)uncomprLen;

    for (;;) {
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        if (err == Z_NEED_DICT) {
            if (d_stream.adler != dictId) {
                fprintf(stderr, "unexpected dictionary");
                exit(1);
            }
            err = PREFIX(inflateSetDictionary)(&d_stream, (const unsigned char*)dictionary,
                                       (int)sizeof(dictionary));
        }
        CHECK_ERR(err, "inflate with dict");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strncmp((char*)uncompr, hello, sizeof(hello))) {
        fprintf(stderr, "bad inflate with dict\n");
        exit(1);
    } else {
        printf("inflate with dictionary: %s\n", (char *)uncompr);
    }
}

/* ===========================================================================
 * Test deflateBound() with small buffers
 */
static void test_deflate_bound(void) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    unsigned int len = (unsigned int)strlen(hello)+1;
    int estimateLen = 0;
    unsigned char *outBuf = NULL;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;
    c_stream.avail_in = len;
    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_out = 0;
    c_stream.next_out = outBuf;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    /* calculate actual output length and update structure */
    estimateLen = PREFIX(deflateBound)(&c_stream, len);
    outBuf = malloc(estimateLen);

    if (outBuf != NULL) {
        /* update zlib configuration */
        c_stream.avail_out = estimateLen;
        c_stream.next_out = outBuf;

        /* do the compression */
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) {
            printf("deflateBound(): OK\n");
        } else {
            CHECK_ERR(err, "deflate");
        }
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(outBuf);
}

/* ===========================================================================
 * Test deflateCopy() with small buffers
 */
static void test_deflate_copy(unsigned char *compr, size_t comprLen) {
    PREFIX3(stream) c_stream, c_stream_copy; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;

    memset(&c_stream, 0, sizeof(c_stream));

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateCopy)(&c_stream_copy, &c_stream);
    CHECK_ERR(err, "deflate_copy");

    if (c_stream.state->status == c_stream_copy.state->status) {
        printf("deflate_copy(): OK\n");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd original");

    err = PREFIX(deflateEnd)(&c_stream_copy);
    CHECK_ERR(err, "deflateEnd copy");
}

/* ===========================================================================
 * Test deflateGetDictionary() with small buffers
 */
static void test_deflate_get_dict(unsigned char *compr, size_t comprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    unsigned char *dictNew = NULL;
    unsigned int *dictLen;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_out = compr;
    c_stream.avail_out = (uInt)comprLen;

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (unsigned int)strlen(hello)+1;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);

    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate should report Z_STREAM_END\n");
        exit(1);
    }

    dictNew = calloc(256, 1);
    dictLen = (unsigned int *)calloc(4, 1);
    err = PREFIX(deflateGetDictionary)(&c_stream, dictNew, dictLen);

    CHECK_ERR(err, "deflateGetDictionary");
    if (err == Z_OK) {
        printf("deflateGetDictionary(): %s\n", dictNew);
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(dictNew);
    free(dictLen);
}

/* ===========================================================================
 * Test deflatePending() with small buffers
 */
static void test_deflate_pending(unsigned char *compr, size_t comprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    int *bits = calloc(256, 1);
    unsigned *ped = calloc(256, 1);
    size_t len = strlen(hello)+1;


    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflatePending)(&c_stream, ped, bits);
    CHECK_ERR(err, "deflatePending");

    if (*bits >= 0 && *bits <= 7) {
        printf("deflatePending(): OK\n");
    } else {
        printf("deflatePending(): error\n");
    }

    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(bits);
    free(ped);
}

/* ===========================================================================
 * Test deflatePrime() wrapping gzip around deflate stream
 */
static void test_deflate_prime(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    PREFIX3(stream) d_stream; /* decompression stream */
    int err;
    size_t len = strlen(hello)+1;
    uint32_t crc = 0;


    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    /* Raw deflate windowBits is -15 */
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    CHECK_ERR(err, "deflateInit2");

    /* Gzip magic number */
    err = PREFIX(deflatePrime)(&c_stream, 16, 0x8b1f);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip compression method (deflate) */
    err = PREFIX(deflatePrime)(&c_stream, 8, 0x08);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip flags (one byte, using two odd bit calls) */
    err = PREFIX(deflatePrime)(&c_stream, 3, 0x0);
    CHECK_ERR(err, "deflatePrime");
    err = PREFIX(deflatePrime)(&c_stream, 5, 0x0);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip modified time */
    err = PREFIX(deflatePrime)(&c_stream, 32, 0x0);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip extra flags */
    err = PREFIX(deflatePrime)(&c_stream, 8, 0x0);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip operating system */
    err = PREFIX(deflatePrime)(&c_stream, 8, 255);
    CHECK_ERR(err, "deflatePrime");

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (uint32_t)len;
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)comprLen;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        CHECK_ERR(err, "deflate");

    /* Gzip uncompressed data crc32 */
    crc = PREFIX(crc32)(0, (const uint8_t *)hello, (uint32_t)len);
    err = PREFIX(deflatePrime)(&c_stream, 32, crc);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip uncompressed data length */
    err = PREFIX(deflatePrime)(&c_stream, 32, (uint32_t)len);
    CHECK_ERR(err, "deflatePrime");

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = (uint32_t)c_stream.total_out;
    d_stream.next_out = uncompr;
    d_stream.avail_out = (uint32_t)uncomprLen;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    /* Inflate with gzip header */
    err = PREFIX(inflateInit2)(&d_stream, MAX_WBITS + 32);
    CHECK_ERR(err, "inflateInit");

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    if (err != Z_BUF_ERROR) {
        CHECK_ERR(err, "inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strcmp((const char *)uncompr, hello) != 0) {
        fprintf(stderr, "bad deflatePrime\n");
        exit(1);
    }

    if (err == Z_OK) {
        printf("deflatePrime(): OK\n");
    }
}

/* ===========================================================================
 * Test deflateSetHeader() with small buffers
 */
static void test_deflate_set_header(unsigned char *compr, size_t comprLen) {
    PREFIX(gz_header) *head = calloc(1, sizeof(PREFIX(gz_header)));
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;


    if (head == NULL) {
        printf("out of memory\n");
        exit(1);
    }

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    /* gzip */
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    CHECK_ERR(err, "deflateInit2");

    head->text = 1;
    err = PREFIX(deflateSetHeader)(&c_stream, head);
    CHECK_ERR(err, "deflateSetHeader");
    if (err == Z_OK) {
        printf("deflateSetHeader(): OK\n");
    }

    c_stream.next_in  = (unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(head);
}

/* ===========================================================================
 * Test deflateTune() with small buffers
 */
static void test_deflate_tune(unsigned char *compr, size_t comprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    int good_length = 3;
    int max_lazy = 5;
    int nice_length = 18;
    int max_chain = 6;
    size_t len = strlen(hello)+1;


    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    err = PREFIX(deflateTune)(&c_stream,(uInt)good_length,(uInt)max_lazy,nice_length,(uInt)max_chain);
    CHECK_ERR(err, "deflateTune");
    if (err == Z_OK) {
        printf("deflateTune(): OK\n");
    }

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Usage:  example [output.gz  [input.gz]]
 */
int main(int argc, const char** argv)
{
    unsigned char *compr, *uncompr;
    z_size_t comprLen = 10000*sizeof(int); /* don't overflow on MSDOS */
    z_size_t uncomprLen = comprLen;
    static const char* myVersion = PREFIX2(VERSION);

    if (zVersion()[0] != myVersion[0]) {
        fprintf(stderr, "incompatible zlib version\n");
        exit(1);

    } else if (strcmp(zVersion(), PREFIX2(VERSION)) != 0) {
        fprintf(stderr, "warning: different zlib version\n");
    }

    printf("zlib version %s, compile flags = 0x%lx\n",
            PREFIX2(VERSION), PREFIX(zlibCompileFlags)());

	zng_lib_init();

    compr    = (unsigned char*)calloc((unsigned int)comprLen, 1);
    uncompr  = (unsigned char*)calloc((unsigned int)uncomprLen, 1);
    /* compr and uncompr are cleared to avoid reading uninitialized
     * data and to ensure that uncompr compresses well.
     */
    if (compr == NULL || uncompr == NULL) {
        printf("out of memory\n");
        exit(1);
    }

    test_compress(compr, comprLen, uncompr, uncomprLen);

    test_gzio((argc > 1 ? argv[1] : TESTFILE),
              uncompr, uncomprLen);

    test_deflate(compr, comprLen);
    test_inflate(compr, comprLen, uncompr, uncomprLen);

    test_large_deflate(compr, comprLen, uncompr, uncomprLen, 0);
    test_large_inflate(compr, comprLen, uncompr, uncomprLen);

#ifndef ZLIB_COMPAT
    test_large_deflate(compr, comprLen, uncompr, uncomprLen, 1);
    test_large_inflate(compr, comprLen, uncompr, uncomprLen);
#endif

    test_flush(compr, &comprLen);
    test_sync(compr, comprLen, uncompr, uncomprLen);
    comprLen = uncomprLen;

    test_dict_deflate(compr, comprLen);
    test_dict_inflate(compr, comprLen, uncompr, uncomprLen);

    test_deflate_bound();
    test_deflate_copy(compr, comprLen);
    test_deflate_get_dict(compr, comprLen);
    test_deflate_set_header(compr, comprLen);
    test_deflate_tune(compr, comprLen);
    test_deflate_pending(compr, comprLen);
    test_deflate_prime(compr, comprLen, uncompr, uncomprLen);

    free(compr);
    free(uncompr);

    return 0;
}
