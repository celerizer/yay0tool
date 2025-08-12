#ifndef YAY0_H
#define YAY0_H

/* Minimal Yay0 decompressor (C89-friendly)
 *
 * API:
 *   int yay0_is_match(const uint8_t *input, size_t input_size);
 *   int yay0_get_decompressed_size(const uint8_t *input, size_t input_size, size_t *out_size);
 *   int yay0_decompress(const uint8_t *input, size_t input_size,
 *                       uint8_t *output, size_t output_size);
 *   int yay0_decompress_headerless(const uint8_t *flag_ptr, size_t flag_len,
 *                                  const uint8_t *comp_ptr, size_t comp_len,
 *                                  const uint8_t *raw_ptr, size_t raw_len,
 *                                  uint8_t *output, size_t output_size);
 *
 * Return codes:
 *   YAY0_OK (0) on success, non-zero on error.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define YAY0_OK               0
#define YAY0_ERR_TRUNCATED    1  /* input region truncated / too short */
#define YAY0_ERR_FORMAT       2  /* header mismatch or malformed */
#define YAY0_ERR_OUTPUT_SMALL 3  /* output buffer too small */
#define YAY0_ERR_BACKREF      4  /* invalid back-reference (distance too large) */

int yay0_is_match(const uint8_t *input, size_t input_size);
int yay0_get_decompressed_size(const uint8_t *input, size_t input_size, size_t *out_size);
int yay0_decompress(const uint8_t *input, size_t input_size,
                    uint8_t *output, size_t output_size);
int yay0_decompress_headerless(const uint8_t *flag_ptr, size_t flag_len,
                               const uint8_t *comp_ptr, size_t comp_len,
                               const uint8_t *raw_ptr, size_t raw_len,
                               uint8_t *output, size_t output_size);
int yay0_compress(const uint8_t *input, size_t input_size,
                  uint8_t **output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif /* YAY0_H */
