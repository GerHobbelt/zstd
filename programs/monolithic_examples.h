
#pragma once

#if defined(BUILD_MONOLITHIC)

#ifdef __cplusplus
extern "C" {
#endif

int zstd_dict_compress_main(int argc, const char** argv);
int zstd_dict_decompress_main(int argc, const char** argv);
int zstd_multi_simple_compress_main(int argc, const char** argv);
int zstd_multi_stream_compress_main(int argc, const char** argv);
int zstd_simple_compress_main(int argc, const char** argv);
int zstd_simple_decompress_main(int argc, const char** argv);
int zstd_stream_compress_main(int argc, const char** argv);
int zstd_stream_compress_threadpool_main(int argc, const char** argv);
int zstd_stream_decompress_main(int argc, const char** argv);
int zstd_stream_mem_usage_main(int argc, const char** argv);
int zstd_main(int argc, const char** argv);
int zstd_fitblk_example_main(int argc, const char** argv);
int zstd_mini_gzip_main(int argc, const char** argv);
int zstd_zwrapbench_main(int argc, const char** argv);

// These require the zstd_zlib_wrapper code to be included in the build:
int zstd_zlib_example_main(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif
