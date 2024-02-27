#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <endian.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/param.h>
#include <limits.h>


struct bitstream
{
    const uint8_t *byte_ptr;
    int bit_off;
    const uint8_t *end;
};

int read_bits(struct bitstream *stream, int count, int *res);
char *b64u_encode(const uint8_t *data, size_t size);
bool b64u_decode(const char *str, uint8_t **p_data, size_t *p_size);