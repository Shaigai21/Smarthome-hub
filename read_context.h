#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct ReadContext
{
    uint8_t *buf;
    size_t size;
    size_t pos;
};

struct trigger
{
    unsigned char op;
    uint64_t value;
    char *name;
};

size_t uleb128_len(uint64_t value);

uint8_t *uleb128_encode(uint8_t *ptr, uint64_t value);

bool uleb128_decode(struct ReadContext *cntx, uint64_t *val);
bool str_read(struct ReadContext *cntx, char **val);
ssize_t trigger_read(struct ReadContext *cntx, struct trigger **arr);
ssize_t str_arr_read(struct ReadContext *cntx, char ***arr);
ssize_t uleb128_arr_read(struct ReadContext *cntx, uint64_t **arr);