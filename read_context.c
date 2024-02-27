#include "read_context.h"

#include <string.h>

enum
{
    LEB128_BYTE_SIZE = 7,
    SEGMENT_BITS = 0x7f,
    CONTINUE_BIT = 0x80
};

size_t
uleb128_len(uint64_t value)
{
    if (!value) {
        return 1;
    }
    size_t size = 0;
    while (value) {
        value >>= LEB128_BYTE_SIZE;
        ++size;
    }
    return size;
}

uint8_t *
uleb128_encode(uint8_t *ptr, uint64_t value)
{
    size_t size = uleb128_len(value);
    for (size_t i = 0; i < size; ++i) {
        ptr[i] = (value & SEGMENT_BITS);
        if (i != size - 1) {
            ptr[i] |= CONTINUE_BIT;
        }
        value >>= LEB128_BYTE_SIZE;
    }
    return ptr + size;
}

bool
uleb128_decode(struct ReadContext *cntx, uint64_t *val)
{
    uint64_t res = 0, shift = 0;
    size_t cnt = cntx->pos;
    while (true) {
        if (cnt > cntx->size - 1) {
            return false;
        }
        uint8_t byte = cntx->buf[cnt++];
        res |= ((uint64_t) (byte & SEGMENT_BITS)) << shift;
        if ((byte & CONTINUE_BIT) == 0) {
            break;
        }
        shift += LEB128_BYTE_SIZE;
        if (shift >= sizeof(shift) * __CHAR_BIT__) {
            return false;
        }
    }
    if (uleb128_len(res) != cnt - cntx->pos) {
        return false;
    }
    cntx->pos = cnt;
    *val = res;

    return true;
}

bool
str_read(struct ReadContext *cntx, char **val)
{
    int str_length = cntx->buf[cntx->pos++];

    *val = calloc(3 + str_length, sizeof(**val));
    if (!*val) {
        return false;
    }
    memcpy(*val, &cntx->buf[cntx->pos], str_length);
    cntx->pos += str_length;
    return true;
}

ssize_t
trigger_read(struct ReadContext *cntx, struct trigger **arr)
{
    ssize_t length = cntx->buf[++cntx->pos];
    if (length < 0) {
        return -1;
    }
    *arr = calloc(length, sizeof(struct trigger));
    if (!*arr) {
        return -1;
    }
    for (int i = 0; i < length; ++i) {
        (*arr)[i].op = cntx->buf[++cntx->pos];
        if (!uleb128_decode(cntx, &(*arr)[i].value)) {
            fprintf(stderr, "Error!\n");
            return -1;
        }
        if (!str_read(cntx, &(*arr)[i].name)) {
            fprintf(stderr, "Error!\n");
            return -1;
        }
    }
    return length;
}

ssize_t
str_arr_read(struct ReadContext *cntx, char ***arr)
{
    ssize_t length = cntx->buf[++cntx->pos];
    if (length < 0) {
        *arr = NULL;
        return -1;
    }
    *arr = calloc(length, sizeof(*arr));
    if (!*arr) {
        return -1;
    }
    for (int i = 0; i < length; ++i) {
        if (!str_read(cntx, &(*arr)[i])) {
            *arr = NULL;
            fprintf(stderr, "Error!\n");
            return -1;
        }
    }
    return length;
}

ssize_t
uleb128_arr_read(struct ReadContext *cntx, uint64_t **arr)
{
    ssize_t length = cntx->buf[++cntx->pos];
    if (length < 0) {
        *arr = NULL;
        return -1;
    }
    *arr = calloc(length, sizeof(*arr));
    if (!*arr) {
        return -1;
    }
    for (int i = 0; i < length; ++i) {
        if (!uleb128_decode(cntx, &(*arr)[i])) {
            *arr = NULL;
            fprintf(stderr, "Error!\n");
            return -1;
        }
    }
    return length;

}