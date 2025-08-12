#ifndef YAY0_H
#define YAY0_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  /* No error */
  YAY0_OK = 0,

  /* Input region truncated or too short */
  YAY0_ERR_TRUNCATED,
  /* Malformed header */
  YAY0_ERR_FORMAT,
  /* Output buffer too small */
  YAY0_ERR_OUTPUT_SMALL,
  /* Invalid back-reference (distance too large) */
  YAY0_ERR_BACKREF,

  YAY0_ERR_SIZE
} yay0_result;

yay0_result yay0_get_decompressed_size(const uint8_t *input, size_t input_size,
  size_t *out_size);

yay0_result yay0_decompress(const uint8_t *input, size_t input_size,
  uint8_t *output, size_t output_size);

yay0_result yay0_decompress_headerless(const uint8_t *flag_ptr, size_t flag_len,
  const uint8_t *comp_ptr, size_t comp_len, const uint8_t *raw_ptr,
  size_t raw_len, uint8_t *output, size_t output_size);

yay0_result yay0_compress(const uint8_t *input, size_t input_size,
  uint8_t **output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif
