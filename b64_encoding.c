#include "b64_encoding.h"

enum
{
    INVALID = 0xFF,
    SEXTET = 6,
    SIZE_CHECKER = 3,
    ADD_SIZE = 10,
    SHIFT_HIGH = 16,
    SHIFT_LOW = 8

};

static const unsigned char pattern[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const unsigned char reverse_table[] = {
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, 62,      INVALID, INVALID, 52,      53,      54,      55,
    56,      57,      58,      59,      60,      61,      INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    0,       1,       2,       3,       4,       5,       6,       7,       8,       9,       10,      11,      12,
    13,      14,      15,      16,      17,      18,      19,      20,      21,      22,      23,      24,      25,
    INVALID, INVALID, INVALID, INVALID, 63,      INVALID, 26,      27,      28,      29,      30,      31,      32,
    33,      34,      35,      36,      37,      38,      39,      40,      41,      42,      43,      44,      45,
    46,      47,      48,      49,      50,      51,      INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,
    INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID};

int
read_bits(struct bitstream *stream, int count, int *res)
{
    const ptrdiff_t bytes_left = stream->end - stream->byte_ptr;
    int ret_val = 1;
    if (stream->bit_off + count > bytes_left * CHAR_BIT) {
        ret_val = 0;
    }
    uint64_t tmp = 0;
    memcpy(&tmp, stream->byte_ptr, MIN(((stream->bit_off + count + CHAR_BIT - 1) / CHAR_BIT), bytes_left));
    tmp = be64toh(tmp);
    *res = (tmp >> (sizeof(uint64_t) * CHAR_BIT - (stream->bit_off + count))) & ((1 << count) - 1);
    stream->byte_ptr += (stream->bit_off + count) / CHAR_BIT;
    stream->bit_off = (stream->bit_off + count) % CHAR_BIT;
    return ret_val;
}

char *
b64u_encode(const uint8_t *data, size_t size)
{
    int res = 0;
    struct bitstream stream = {data, 0, &data[size]};

    char *str = calloc((CHAR_BIT * size + SEXTET - 1 + SEXTET) / SEXTET, sizeof(*str));
    if (!str) {
        return NULL;
    }
    size_t cnt = 0;
    while (read_bits(&stream, SEXTET, &res)) {
        str[cnt++] = pattern[res];
    }
    if (size % SIZE_CHECKER) {
        str[cnt] = pattern[res];
    }
    return str;
}

bool
b64u_decode(const char *str, uint8_t **p_data, size_t *p_size)
{
    char sym;
    unsigned char i = 0;
    size_t cnt = 0;
    unsigned buf = 0;
    int cnt_syms = 0;
    size_t cur_data = 0;
    uint8_t *arr = calloc((strlen(str) * SEXTET + SEXTET + 1 + CHAR_BIT) / CHAR_BIT, sizeof(*arr));
    while ((sym = str[cnt++]) > 0) {
        if (isspace(sym)) {
            continue;
        }
        i = reverse_table[(size_t) sym];
        if (i == INVALID) {
            free(arr);
            return false;
        }
        ++cnt_syms;
        buf <<= SEXTET;
        buf |= i;
        if (cnt_syms == SIZE_CHECKER + 1) {
            cnt_syms = 0;
            arr[cur_data++] = buf >> SHIFT_HIGH;
            arr[cur_data++] = (buf >> SHIFT_LOW) & INVALID;
            arr[cur_data++] = buf & INVALID;
            buf = 0;
        }
    }
    if (cnt_syms == SIZE_CHECKER) {
        arr[cur_data++] = buf >> ADD_SIZE;
        arr[cur_data++] = (buf >> (SIZE_CHECKER - 1)) & INVALID;
    } else if (cnt_syms == SIZE_CHECKER - 1) {
        arr[cur_data++] = buf >> (SIZE_CHECKER + 1);
    }

    *p_size = cur_data;
    *p_data = arr;
    return true;
}
