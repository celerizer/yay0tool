#include <string.h>
#include <stdlib.h>

#include "yay0.h"

#ifndef YAY0_BIG_ENDIAN
  #if defined(__N64__)
    #define YAY0_BIG_ENDIAN 1
  #else
    #define YAY0_BIG_ENDIAN 0
  #endif
#endif

static uint32_t read_be_u32(const uint8_t *p)
{
#if YAY0_BIG_ENDIAN
  return *(const uint32_t*)p;
#else
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8)  | ((uint32_t)p[3]);
#endif
}

static void be_write_u32(uint8_t *dst, unsigned int val)
{
#if YAY0_BIG_ENDIAN
  *(uint32_t*)dst = val;
#else
  dst[0] = (uint8_t)((val >> 24) & 0xFF);
  dst[1] = (uint8_t)((val >> 16) & 0xFF);
  dst[2] = (uint8_t)((val >> 8) & 0xFF);
  dst[3] = (uint8_t)(val & 0xFF);
#endif
}

static void be_write_u16(uint8_t *dst, unsigned short val)
{
#if YAY0_BIG_ENDIAN
  *(uint16_t*)dst = val;
#else
  dst[0] = (uint8_t)((val >> 8) & 0xFF);
  dst[1] = (uint8_t)(val & 0xFF);
#endif
}

typedef struct
{
  const uint8_t *data;
  size_t len;
  size_t byte_pos;
  int bit_mask; /* current mask, 0 means need to load next byte */
} yay0_flag_t;

static void flagreader_init(yay0_flag_t *fr, const uint8_t *ptr, size_t len)
{
  fr->data = ptr;
  fr->len = len;
  fr->byte_pos = 0;
  fr->bit_mask = 0;
}

/* return 1 for bit=1, 0 for bit=0; if truncated -> returns -1 */
static int flagreader_readbit(yay0_flag_t *fr)
{
  unsigned char current;
  int bit;

  if (fr->bit_mask == 0)
  {
    if (fr->byte_pos >= fr->len)
      return -1;
    fr->bit_mask = 0x80;
  }
  current = fr->data[fr->byte_pos];
  bit = (current & fr->bit_mask) ? 1 : 0;
  fr->bit_mask >>= 1;
  if (fr->bit_mask == 0)
    fr->byte_pos++;

  return bit;
}

typedef struct
{
  const uint8_t *data;
  size_t len;
  size_t pos;
} yay0_region_t;

static void rr_init(yay0_region_t *rr, const uint8_t *data, size_t len)
{
  rr->data = data;
  rr->len = len;
  rr->pos = 0;
}

/* read a single byte; returns -1 on truncation, else 0..255 */
static int rr_read_u8(yay0_region_t *rr)
{
  if (rr->pos >= rr->len) return -1;
  return rr->data[rr->pos++];
}

int yay0_decompress_headerless(const uint8_t *flag_ptr, size_t flag_len,
  const uint8_t *comp_ptr, size_t comp_len, const uint8_t *raw_ptr,
  size_t raw_len, uint8_t *output, size_t output_size)
{
  yay0_flag_t flags;
  yay0_region_t comp, raw;
  size_t out_written = 0;
  int bit;

  if (!flag_ptr || !comp_ptr || !raw_ptr || !output)
    return YAY0_ERR_FORMAT;

  flagreader_init(&flags, flag_ptr, flag_len);
  rr_init(&comp, comp_ptr, comp_len);
  rr_init(&raw, raw_ptr, raw_len);

  while (out_written < output_size)
  {
    bit = flagreader_readbit(&flags);
    if (bit < 0)
      return YAY0_ERR_TRUNCATED;

    if (bit)
    {
      /* literal: read one byte from raw */
      int v = rr_read_u8(&raw);
      if (v < 0) return YAY0_ERR_TRUNCATED;
      output[out_written++] = (uint8_t)v;
    }
    else
    {
      int w, b2, distance, length, i;
      size_t src_index;

      /* backreference */
      w = rr_read_u8(&comp);
      if (w < 0)
        return YAY0_ERR_TRUNCATED;

      b2 = rr_read_u8(&comp);
      if (b2 < 0)
        return YAY0_ERR_TRUNCATED;

      /* distance: lower 12 bits + 1, length: high 4 bits */
      distance = (((w & 0x0F) << 8) | b2) + 1;
      length = (w >> 4) & 0x0F;

      if (length == 0)
      {
        int ext = rr_read_u8(&raw);

        if (ext < 0)
          return YAY0_ERR_TRUNCATED;
        length = ext + 0x12;
      }
      else
        length += 2;

      /* Validate backreference: must have enough previously output bytes */
      if (distance > (int)out_written)
        return YAY0_ERR_BACKREF;

      /* copy from already written output at (out_written - distance) */
      src_index = out_written - (size_t)distance;
      
      /* copy byte-by-byte to correctly handle overlapping copying */
      for (i = 0; i < length; ++i)
      {
        if (out_written >= output_size)
          break;
        else
        {
          output[out_written] = output[src_index];
          ++out_written;
          ++src_index;
        }
      }
    }
  }

  return YAY0_OK;
}

int yay0_decompress(const uint8_t *input, size_t input_size, uint8_t *output,
  size_t output_size)
{
  uint32_t decom_size, comp_off, raw_off, min_off;
  const uint8_t *flag_ptr, *comp_ptr, *raw_ptr;
  size_t flag_len = 0, comp_len, raw_len;
  
  /* minimal header: 16 bytes: "Yay0" + 3 * u32 */
  const size_t HEADER_SIZE = 16;
  if (!input || input_size < HEADER_SIZE)
    return YAY0_ERR_TRUNCATED;

  /* check magic */
  if (!yay0_is_match(input, input_size))
    return YAY0_ERR_FORMAT;

  /* read decompressed size */
  decom_size = read_be_u32(input + 4);
  if ((size_t)decom_size > output_size)
    return YAY0_ERR_OUTPUT_SMALL;

  /* offsets are absolute offsets from start of file */
  comp_off = read_be_u32(input + 8);
  raw_off = read_be_u32(input + 12);

  /* validate offsets */
  if (comp_off > input_size || raw_off > input_size)
    return YAY0_ERR_TRUNCATED;

  /* compute pointers & lengths relative to entire input buffer */
  flag_ptr = input + HEADER_SIZE;
  
  /**
   * flag region runs from offset HEADER_SIZE up to the lesser of comp_off and
   * raw_off (whichever is min)
   */
  min_off = (comp_off < raw_off) ? comp_off : raw_off;
  if (min_off < HEADER_SIZE)
    return YAY0_ERR_FORMAT;

  flag_len = (size_t)(min_off - HEADER_SIZE);
  comp_ptr = input + comp_off;
  comp_len = input_size - comp_off;
  raw_ptr = input + raw_off;
  raw_len = input_size - raw_off;

  return yay0_decompress_headerless(flag_ptr, flag_len, comp_ptr, comp_len,
    raw_ptr, raw_len, output, (size_t)decom_size);
}

int yay0_is_match(const uint8_t *input, size_t input_size)
{
  if (!input || input_size < 4)
    return 0;
  else
    return input[0] == 'Y' &&
           input[1] == 'a' &&
           input[2] == 'y' &&
           input[3] == '0';
}

int yay0_get_decompressed_size(const uint8_t *input, size_t input_size,
  size_t *out_size)
{
  if (!input || input_size < 8 || !out_size)
    return YAY0_ERR_TRUNCATED;
  else if (!yay0_is_match(input, input_size))
    return YAY0_ERR_FORMAT;
  else
  {
    *out_size = (size_t)read_be_u32(input + 4);
    return YAY0_OK;
  }
}

static const uint8_t *enc_bz = NULL;
static int enc_insize = 0;

/* Boyer-Moore-like skip table used by mischarsearch() */
static unsigned short enc_skip[256];

static void enc_initskip(const unsigned char *pattern, int len)
{
  int i;
  for (i = 0; i < 256; ++i)
    enc_skip[i] = (unsigned short)len;
  for (i = 0; i < len; ++i)
    enc_skip[(unsigned char)pattern[i]] = (unsigned short)(len - i - 1);
}

/* Find the first occurrence of 'pattern' (length patternlen) in data
   (length datalen) using a simple skip heuristic. Returns index within
   'data' (0..datalen-1) where the pattern starts, or datalen if not found. */
static int enc_mischarsearch(const unsigned char *pattern, int patternlen,
                             const unsigned char *data, int datalen)
{
  int result = datalen;
  int i, j, v6;

  if (patternlen <= datalen) {
    enc_initskip(pattern, patternlen);
    i = patternlen - 1;
    for (;;) {
      if (pattern[patternlen - 1] == data[i]) {
        --i;
        j = patternlen - 2;
        if (j < 0) {
          return i + 1;
        }
        while (pattern[j] == data[i]) {
          --i;
          --j;
          if (j < 0)
            return i + 1;
        }
        v6 = patternlen - j;
        if (enc_skip[data[i]] > (unsigned)v6)
          v6 = enc_skip[data[i]];
        /* increment i by v6 below via loop increment */
        i += v6;
      } else {
        v6 = enc_skip[data[i]];
        i += v6;
      }
      if (i >= datalen)
        break;
    }
  }
  return result;
}

#define YAY0_MATCH_LEN_MAX 273

static void enc_search(unsigned cur_pos, int buf_end, int *match_pos_out,
  unsigned *match_len_out)
{
  unsigned match_len = 3;        /* Starting minimum match length */
  unsigned search_start = 0;     /* Earliest position to search from */
  unsigned mismatch_offset;      /* Where mismatch happens in search window */
  unsigned best_match_pos = 0;   /* Best match position found so far */
  unsigned max_match_len;        /* Maximum possible match length */

  /* Limit search window to 4 KB before current position */
  if (cur_pos > 0x1000)
    search_start = cur_pos - 0x1000;

  /* Calculate the maximum match length possible based on remaining bytes */
  max_match_len = YAY0_MATCH_LEN_MAX;
  if ((unsigned)(buf_end - (int)cur_pos) <= 0x111)
      max_match_len = (unsigned)(buf_end - (int)cur_pos);

  /* Skip if not enough data for a match */
  if (max_match_len < match_len)
  {
    *match_len_out = 0;
    *match_pos_out = 0;

    return;
  }

  /* Search backwards for the best match */
  while (cur_pos > search_start)
  {
    mismatch_offset = (unsigned)enc_mischarsearch(
      &enc_bz[cur_pos],
      match_len,
      &enc_bz[search_start],
      match_len + cur_pos - search_start
    );

    /* No more candidates in range */
    if (mismatch_offset >= cur_pos - search_start)
      break;

    /* Extend match length as far as possible */
    while (max_match_len > match_len &&
           enc_bz[match_len + search_start + mismatch_offset] == enc_bz[match_len + cur_pos])
      ++match_len;

    /* Found the longest possible match */
    if (match_len == max_match_len)
    {
      *match_pos_out = (int)(search_start + mismatch_offset);
      *match_len_out = match_len;

      return;
    }

    /* Store this as current best match */
    best_match_pos = search_start + mismatch_offset;
    ++match_len;
    search_start += mismatch_offset + 1;

    if (cur_pos <= search_start)
      break;
  }

  /* Return result if match length is valid */
  if (match_len > 3)
  {
    *match_pos_out = best_match_pos;
    *match_len_out = match_len - 1;
  }
  else
  {
    *match_len_out = 0;
    *match_pos_out = 0;
  }
}

int yay0_compress(const uint8_t *input, size_t input_size,
                  uint8_t **output, size_t *output_size)
{
  /* local variables modeled after your program */
  unsigned int v0;
  unsigned int v1;
  int a3;
  unsigned int a4;
  unsigned int v6;
  unsigned int v7;
  int v8;
  int v2;
  int v3;
  unsigned char v5;

  /* dynamic arrays */
  unsigned int *cmd = NULL;      /* 32-bit flag words */
  unsigned short *pol = NULL;    /* compressed tokens (words) */
  unsigned char *def = NULL;     /* literals and extra lengths */

  unsigned int dp = 0;
  unsigned int pp = 0;
  unsigned int cp = 0;
  unsigned int npp = 4096;
  unsigned int ndp = 4096;
  unsigned int ncp = 4096;
  unsigned int insz;
  size_t total_size;
  uint8_t *outbuf;
  size_t outpos;
  unsigned int i;

  /* validate args */
  if (!input || !output || !output_size) return YAY0_ERR_FORMAT;
  if (input_size > INT_MAX) return YAY0_ERR_FORMAT; /* our code uses int in places */

  /* init encoder state */
  enc_bz = input;
  enc_insize = (int)input_size;
  insz = (unsigned int)enc_insize;

  dp = 0;
  pp = 0;
  cp = 0;
  npp = 4096;
  ndp = 4096;
  ncp = 4096;

  /* allocate initial buffers (same sizes as your program used): */
  cmd = (unsigned int *) calloc(0x4000u, sizeof(unsigned int)); /* 0x4000 * 4 bytes like your code */
  if (!cmd) return YAY0_ERR_FORMAT;
  pol = (unsigned short *) malloc(2 * npp);
  if (!pol) { free(cmd); return YAY0_ERR_FORMAT; }
  def = (unsigned char *) malloc(4 * ndp);
  if (!def) { free(cmd); free(pol); return YAY0_ERR_FORMAT; }

  v0 = 0;
  v6 = 1024u;
  v1 = 0x80000000u;

  /* ensure first cmd entry exists and is zero-initialized */
  cmd[0] = 0;

  while ((unsigned)v0 < insz) {
    if (v6 < v0)
      v6 += 1024u;
    enc_search(v0, enc_insize, &a3, &a4);
    if (a4 <= 2u) {
      cmd[cp] |= v1;
      def[dp++] = enc_bz[v0++];
      if (ndp == dp) {
        ndp = dp + 4096u;
        def = (unsigned char *) realloc(def, (size_t)(dp + 4096u));
        if (!def) goto err_cleanup;
      }
    } else {
      enc_search(v0 + 1u, enc_insize, &v8, &v7);
      if (v7 > a4 + 1u) {
        cmd[cp] |= v1;
        def[dp++] = enc_bz[v0++];
        if (ndp == dp) {
          ndp = dp + 4096u;
          def = (unsigned char *) realloc(def, (size_t)(dp + 4096u));
          if (!def) goto err_cleanup;
        }
        v1 >>= 1;
        if (!v1) {
          v1 = 0x80000000u;
          v2 = (int)cp++;
          if (cp == ncp) {
            ncp = (unsigned int)(v2 + 1025);
            cmd = (unsigned int *) realloc(cmd, 4 * (size_t)(v2 + 1025));
            if (!cmd) goto err_cleanup;
          }
          cmd[cp] = 0;
        }
        a4 = v7;
        a3 = v8;
      }
      v3 = (int)(v0 - a3 - 1);
      a3 = v0 - a3 - 1;
      v5 = (unsigned char)a4;
      if (a4 > 0x11u) {
        /* store long form: distance then extra length byte in def */
        if (pp >= npp) {
          npp = pp + 4096u;
          pol = (unsigned short *) realloc(pol, 2 * (size_t)(pp + 4096u));
          if (!pol) goto err_cleanup;
        }
        pol[pp++] = (unsigned short)v3;
        def[dp++] = (unsigned char)(v5 - 18);
        if (ndp == dp) {
          ndp = dp + 4096u;
          def = (unsigned char *) realloc(def, (size_t)(dp + 4096u));
          if (!def) goto err_cleanup;
        }
      } else {
        /* store packed 16-bit token: distance (low 12) | ((len-2) << 12) */
        if (pp >= npp) {
          npp = pp + 4096u;
          pol = (unsigned short *) realloc(pol, 2 * (size_t)(pp + 4096u));
          if (!pol) goto err_cleanup;
        }
        pol[pp++] = (unsigned short)(v3 | ((((unsigned short)a4) - 2) << 12));
      }
      v0 += a4;
    }
    v1 >>= 1;
    if (!v1) {
      v1 = 0x80000000u;
      /* start new flag word */
      {
        int tmp = (int)cp++;
        if (cp == ncp) {
          ncp = (unsigned int)(tmp + 1025);
          cmd = (unsigned int *) realloc(cmd, 4 * (size_t)(tmp + 1025));
          if (!cmd) goto err_cleanup;
        }
        cmd[cp] = 0;
      }
    }
  }

  if (v1 != 0x80000000u)
    ++cp;

  /* compute output size and build final buffer:
       header (16) + cmd words (4*cp) + pol words (2*pp) + def bytes (dp) */
  total_size = 16 + 4 * (size_t)cp + 2 * (size_t)pp + (size_t)dp;
  outbuf = (uint8_t*)malloc(total_size);
  if (!outbuf)
    goto err_cleanup;

  /* Write header */
  memcpy(outbuf, "Yay0", 4);
  be_write_u32(outbuf + 4, (unsigned int)insz);
  /* compressedDataPointer (offset to pol area) = 4*cp + 16 */
  be_write_u32(outbuf + 8, 4u * cp + 16u);
  /* uncompressedDataPointer (offset to def area) = 2*pp + 4*cp + 16 */
  be_write_u32(outbuf + 12, 2u * pp + 4u * cp + 16u);

  /* write cmd[] (flag words) big-endian starting at offset 16 */
  outpos = 16;
  for (i = 0; i < cp; ++i)
  {
    be_write_u32(outbuf + outpos, cmd[i]);
    outpos += 4;
  }

  /* write pol[] (compressed tokens) big-endian */
  for (i = 0; i < pp; ++i)
  {
    be_write_u16(outbuf + outpos, pol[i]);
    outpos += 2;
  }

  /* write def[] (literals and extra length bytes) */
  if (dp > 0)
  {
    memcpy(outbuf + outpos, def, (size_t)dp);
    outpos += dp;
  }

  /* sanity check */
  if (outpos != total_size)
  {
    free(outbuf);
    goto err_cleanup;
  }

  /* success: return buffer */
  *output = outbuf;
  *output_size = total_size;

  /* cleanup */
  free(cmd);
  free(pol);
  free(def);
  return YAY0_OK;

err_cleanup:
  if (cmd) free(cmd);
  if (pol) free(pol);
  if (def) free(def);
  return YAY0_ERR_FORMAT;
}
