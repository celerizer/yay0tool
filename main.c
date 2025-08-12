#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yay0.h"

static unsigned char *read_file(const char *path, size_t *out_size)
{
    FILE *f;
    unsigned char *buf;
    long len;

    *out_size = 0;
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    buf = (unsigned char *)malloc((size_t)len);
    if (!buf) { fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "Error: failed to read %s\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = (size_t)len;
    return buf;
}

static int write_file(const char *path, const unsigned char *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s for writing\n", path);
        return 0;
    }
    if (fwrite(data, 1, size, f) != size) {
        fprintf(stderr, "Error: failed to write to %s\n", path);
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    unsigned char *input_data = NULL, *output_data = NULL;
    size_t input_size = 0, output_size = 0;
    int ret;

    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <encode|decode> <inputfile> <outputfile>\n", argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    const char *input_path = argv[2];
    const char *output_path = argv[3];

    if (strcmp(mode, "decode") == 0) {
        /* Decode: decompress Yay0 */

        input_data = read_file(input_path, &input_size);
        if (!input_data) return 1;

        if (!yay0_is_match(input_data, input_size)) {
            fprintf(stderr, "Error: %s is not a Yay0 file\n", input_path);
            free(input_data);
            return 1;
        }

        ret = yay0_get_decompressed_size(input_data, input_size, &output_size);
        if (ret != YAY0_OK) {
            fprintf(stderr, "Error: failed to get decompressed size (code %d)\n", ret);
            free(input_data);
            return 1;
        }

        output_data = (unsigned char *)malloc(output_size);
        if (!output_data) {
            fprintf(stderr, "Error: cannot allocate %lu bytes\n", (unsigned long)output_size);
            free(input_data);
            return 1;
        }

        ret = yay0_decompress(input_data, input_size, output_data, output_size);
        if (ret != YAY0_OK) {
            fprintf(stderr, "Error: decompression failed (code %d)\n", ret);
            free(input_data);
            free(output_data);
            return 1;
        }

        if (!write_file(output_path, output_data, output_size)) {
            free(input_data);
            free(output_data);
            return 1;
        }

        printf("Decompressed %s -> %s (%lu bytes)\n",
               input_path, output_path, (unsigned long)output_size);

    } else if (strcmp(mode, "encode") == 0) {
        /* Encode: compress into Yay0 */

	output_size = input_size * 2;
        input_data = read_file(input_path, &input_size);
        if (!input_data) return 1;

        output_data = (unsigned char *)malloc(output_size);
        if (!output_data) {
            fprintf(stderr, "Error: cannot allocate %lu bytes\n", (unsigned long)output_size);
            free(input_data);
            return 1;
        }

        ret = yay0_compress(input_data, input_size, &output_data, &output_size);
        if (ret != YAY0_OK) {
            fprintf(stderr, "Error: compression failed (code %d)\n", ret);
            free(input_data);
            free(output_data);
            return 1;
        }

        if (!write_file(output_path, output_data, output_size)) {
            free(input_data);
            free(output_data);
            return 1;
        }

        printf("Compressed %s -> %s (%lu bytes)\n",
               input_path, output_path, (unsigned long)output_size);

    } else {
        fprintf(stderr, "Invalid mode '%s', use encode or decode\n", mode);
        return 1;
    }

    free(input_data);
    free(output_data);
    return 0;
}
