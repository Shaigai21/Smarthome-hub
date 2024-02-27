#include "crc8_encoding.h"
#include "read_context.h"
#include "b64_encoding.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <stdbool.h>
#include <sys/param.h>
#include <limits.h>
#include <curl/curl.h>
#include <assert.h>

enum
{
    BROADCAST = 0X3FFF,
    MAX_PACKET_SIZE = 33
};

enum device_type
{
    HUB = 0x01,
    SENSOR = 0x02,
    SWITCH = 0x03,
    LAMP = 0x04,
    SOCKET = 0x05,
    CLOCK = 0x06
};

enum cmd_type
{
    WHOISHERE = 0x01,
    IAMHERE = 0x02,
    GETSTATUS = 0x03,
    STATUS = 0x04,
    SETSTATUS = 0x05,
    TICK = 0x06
};

struct device
{
    char *dev_name;
    union
    {
        struct
        {
            size_t size;
            char **strings;
        } names;

        struct
        {
            unsigned char num;
            struct
            {
                size_t size;
                struct trigger *arr;
            } triggers;
        } sensor;
    };
};

struct status
{
    union
    {
        unsigned char val;
        struct
        {
            ssize_t size;
            uint64_t *arr;
        } values;
        uint64_t timestamp;
    };
};

struct bvec
{
    char *memory;
    size_t size;
};

struct payload
{
    uint64_t src;
    uint64_t dst;
    uint64_t serial;
    enum device_type dev_type;
    enum cmd_type cmd;
    union
    {
        struct device cmd_here;
        struct status st;
    };
};

static size_t
write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct bvec *mem = (struct bvec *) userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

ssize_t
read_packet(uint8_t *data, struct payload *pld, size_t size)
{
    *pld = (struct payload){};
    uint8_t length = data[0];
    if (length + 2 > size) {
        printf("%d %ld\n", length, size);
        assert(false);
    }
    uint8_t crc8 = data[length + 1];
    if (crc8 != compute_crc8(&data[1], length)) {
        assert(false);
    }

    struct ReadContext ctx = {&data[1], length, 0};
    if (!uleb128_decode(&ctx, &pld->src)) {
        assert(false);
    }

    if (!uleb128_decode(&ctx, &pld->dst)) {
        assert(false);
    }

    if (!uleb128_decode(&ctx, &pld->serial)) {
        assert(false);
    }

    pld->dev_type = data[++ctx.pos];
    pld->cmd = data[++ctx.pos];

    switch (pld->cmd) {

    case WHOISHERE:
    case IAMHERE: {
        if (pld->dst != BROADCAST) {
            assert(false);
        }
        if (!str_read(&ctx, &pld->cmd_here.dev_name)) {
            assert(false);
        }
        switch (pld->dev_type) {

        case HUB:
        case LAMP:
        case SOCKET:
        case CLOCK: {
            break;
        }

        case SENSOR: {
            pld->cmd_here.sensor.num = data[ctx.pos++];
            pld->cmd_here.sensor.triggers.size = trigger_read(&ctx, &pld->cmd_here.sensor.triggers.arr);
            if (pld->cmd_here.sensor.triggers.size == -1) {
                assert(false);
            }
            break;
        }

        case SWITCH: {
            pld->cmd_here.names.size = str_arr_read(&ctx, &pld->cmd_here.names.strings);
            if (pld->cmd_here.names.size == -1) {
                assert(false);
            }
            break;
        }
        default: {
            assert(false);
        }
        }
        break;
    }
    case GETSTATUS: {
        if (pld->dst == BROADCAST) {
            assert(false);
        }
        break;
    }
    case SETSTATUS: {
        if (pld->dst == BROADCAST) {
            assert(false);
        }
        break;
    }
    case TICK: {
        if (!uleb128_decode(&ctx, &pld->st.timestamp)) {
            assert(false);
        }
        break;
    }
    case STATUS: {
        if (pld->dst == BROADCAST) {
            assert(false);
        }
        switch (pld->dev_type) {
        case LAMP:
        case SOCKET:
        case SWITCH: {
            pld->st.val = data[++ctx.pos];
        }
        case HUB:
        case CLOCK: {
            assert(false);
        }
        case SENSOR: {
            pld->st.values.size = uleb128_arr_read(&ctx, &pld->st.values.arr);
            if (pld->st.values.size == -1) {
                assert(false);
            }
            break;
        }
        default: {
            assert(false);
        }
        }
        break;
    }
    default: {
        assert(false);
    }
    }
    return length + 2;
}
ssize_t
write_packet(uint8_t *data, struct payload *pld, size_t size)
{
    uint8_t *ptr = uleb128_encode(&data[1], pld->src);
    ptr = uleb128_encode(ptr, pld->dst);
    ptr = uleb128_encode(ptr, pld->serial);
    *ptr = pld->dev_type;
    *(ptr + 1) = pld->cmd;
    ptr += 2;

    if (pld->cmd == SETSTATUS) {
        *ptr = pld->st.val;
        ptr++;
    }
    data[0] = (uint8_t) (ptr - &data[1]);
    *ptr = compute_crc8(&data[1], data[0]);

    return data[0] + 2;
}

int
main(int argc, char **argv)
{
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *handler = curl_easy_init();
    struct bvec chunk;

    curl_easy_setopt(handler, CURLOPT_URL, argv[1]);
    curl_easy_setopt(handler, CURLOPT_WRITEFUNCTION, write_memory_callback);

    struct payload pack = {1, 4098, 16, 4, 3, .st.val = 1};
    uint8_t data[MAX_PACKET_SIZE] = {};
    ssize_t len_data = write_packet(data, &pack, MAX_PACKET_SIZE);
    char *enc_pack = b64u_encode(data, len_data);

    curl_easy_setopt(handler, CURLOPT_POSTFIELDS, enc_pack);
    curl_easy_setopt(handler, CURLOPT_WRITEDATA, (void *) &chunk);
    CURLcode res = curl_easy_perform(handler);
    if (res != CURLE_OK) {
        printf("pizdec\n");
        goto clean;
    }
    printf("%s\n", chunk.memory);

    /*     uint8_t *data;
        size_t p_size;
        if (!b64u_decode(chunk.memory, &data, &p_size)) {
            printf("b64 err\n");
        } */

    /* ssize_t first = read_packet(data, &pack, chunk.size);
    if (first < 0) {
        printf("aaaaaaaaaaaaa\n");
    } else {
        printf("%lu %lu %lu %d %d\n", pack.src, pack.dst, pack.serial, pack.dev_type, pack.cmd);
        printf("%ld\n", pack.st.timestamp);
    }
    ssize_t second = read_packet(data + first, &pack, chunk.size - first);
    if (second < 0) {
        printf("aaaaaaaaaaaaa\n");
    } else {
        printf("%lu %lu %lu %d %d\n", pack.src, pack.dst, pack.serial, pack.dev_type, pack.cmd);
        printf("%s\n", pack.cmd_here.dev_name);
    } */

clean:
    curl_easy_cleanup(handler);
    free(chunk.memory);
    free(enc_pack);
    curl_global_cleanup();
}
