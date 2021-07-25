
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
int zlib_deflate_quick_bi_valid_test_main();
int zlib_deflate_quick_block_open_test_main();
int zlib_example_main(int argc, const char** argv);
int zlib_mini_deflate_main(int argc, const char** argv);
int zlib_mini_gzip_main(int argc, const char** argv);
int zlib_switchlevels_main(int argc, const char** argv);
int zlib_mk_crc32_tables_main(int argc, const char** argv);
int zlib_adler32_test_main();
int zlib_hash_head_0_test_main();
int zlib_infcover_test_main();
int zlib_mk_fixed_table_main();
int zlib_mk_trees_header_main();

#ifdef __cplusplus
}
#endif

#endif
