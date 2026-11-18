#include "whale_band.h"

#include <assert.h>
#include <string.h>

static whale_band_workspace_t workspace;

static void check_deflate_blocks(void) {
    const uint8_t stored_then_fixed[] = {
        0x00, 1, 0, 0xfe, 0xff, '!',
        0x2b, 0xcf, 0x48, 0xcc, 0x49, 0x2d, 0x27, 0x4c, 0x00, 0x00
    };
    const char expected[] = "!whalewhalewhalewhalewhalewhalewhalewhale";
    const uint8_t dynamic[] = {
        0x2d, 0x8a, 0x41, 0x0e, 0x00, 0x30, 0x08, 0xc2, 0x80, 0xf9, 0xff, 0x37,
        0x4b, 0xcd, 0x84, 0x43, 0x49, 0x95, 0xdc, 0xd0, 0x5e, 0x0a, 0xcf, 0x1a,
        0x80, 0xe5, 0x73, 0xdf, 0xa7, 0xc3, 0xc1, 0x9e, 0x2e, 0xf2, 0xb0
    };
    const uint8_t expected_dynamic[] = {
        0,0,1,0,1,0,0,1,0,0,0,0,0,2,0,0,1,3,1,0,4,0,2,0,0,0,0,2,0,1,1,0,
        1,0,0,0,1,0,0,1,0,0,2,1,0,1,1,2,1,0,4,0,0,2,0,0,0,1,2,1,2,0,1,1
    };
    uint8_t out[sizeof(expected_dynamic) + 1];
    out[sizeof(expected_dynamic)] = 0xa5;
    assert(whale_band_inflate(dynamic, sizeof(dynamic), out, sizeof(expected_dynamic), &workspace));
    assert(memcmp(out, expected_dynamic, sizeof(expected_dynamic)) == 0);
    assert(out[sizeof(expected_dynamic)] == 0xa5);
    for (size_t len = 0; len < sizeof(dynamic); len++) {
        assert(!whale_band_inflate(dynamic, len, out, sizeof(expected_dynamic), &workspace));
    }
    assert(whale_band_inflate(stored_then_fixed, sizeof(stored_then_fixed), out, sizeof(expected) - 1, &workspace));
    assert(memcmp(out, expected, sizeof(expected) - 1) == 0);
    assert(!whale_band_inflate(stored_then_fixed, sizeof(stored_then_fixed), out, sizeof(expected), &workspace));
    out[2] = 0xa5;
    assert(!whale_band_inflate(stored_then_fixed, sizeof(stored_then_fixed), out, 2, &workspace));
    assert(out[2] == 0xa5);
    const uint8_t bad_stored[] = {0x01, 1, 0, 0, 0, 'x'};
    const uint8_t reserved_type[] = {0x07};
    const uint8_t extra_byte[] = {0x03, 0x00, 0x00}; /* empty fixed block, then trailing data */
    assert(!whale_band_inflate(bad_stored, sizeof(bad_stored), out, 1, &workspace));
    assert(!whale_band_inflate(reserved_type, sizeof(reserved_type), out, 0, &workspace));
    assert(!whale_band_inflate(extra_byte, sizeof(extra_byte), out, 0, &workspace));
}

enum { WIDTH = 120, HEIGHT = 17 };
static const uint8_t bands[] = {
    0, 0, 17, 0, 23, 0,
    0x63, 0x64, 0x1c, 0x05, 0xa3, 0x60, 0x14, 0x8c, 0x82,
    0x51, 0x30, 0x0a, 0x46, 0x01, 0xbd, 0x01, 0x00,
    0x63, 0x62, 0x1a, 0x18, 0x00, 0x00
};

typedef struct {
    size_t bytes;
    unsigned chunks;
    unsigned fail_at;
} bmp_sink_t;

static bool check_bmp_chunk(void *context, const uint8_t *data, size_t length) {
    bmp_sink_t *sink = context;
    if (++sink->chunks == sink->fail_at) return false;
    if (sink->chunks == 1) {
        assert(length == WHALE_BG_BMP_HEADER_BYTES && data[0] == 'B' && data[1] == 'M');
        assert(data[18] == WIDTH && data[22] == HEIGHT && data[28] == 8);
        const uint8_t palette_entry[] = {0x12, 0x34, 0x56, 0};
        assert(memcmp(data + 54 + 4, palette_entry, sizeof(palette_entry)) == 0);
    } else {
        /* The last (partial) source band comes first in the BMP. */
        assert(length == (sink->chunks == 2 ? WIDTH : WIDTH * WHALE_BAND_ROWS));
        for (size_t i = 0; i < length; i++) assert(data[i] == (sink->chunks == 2 ? 2 : 1));
    }
    sink->bytes += length;
    return true;
}

static void check_bands_and_bmp(void) {
    uint8_t scratch[WIDTH * WHALE_BAND_ROWS];
    uint8_t asset[WHALE_BG_PALETTE_BYTES + sizeof(bands)] = {[3] = 0x12, 0x34, 0x56};
    memcpy(asset + WHALE_BG_PALETTE_BYTES, bands, sizeof(bands));
    assert(whale_band_decode_indices(bands, sizeof(bands), WIDTH, HEIGHT, 1, scratch, &workspace));
    for (unsigned x = 0; x < WIDTH; x++) assert(scratch[x] == 2);
    assert(!whale_band_decode_indices(bands, sizeof(bands) - 1, WIDTH, HEIGHT, 1, scratch, &workspace));
    assert(!whale_band_decode_indices(bands, sizeof(bands), WIDTH, HEIGHT, 2, scratch, &workspace));
    assert(!whale_band_decode_indices(bands, sizeof(bands), WIDTH + 1, HEIGHT, 0, scratch, &workspace));
    const uint8_t reversed_offsets[] = {7, 0, 3, 0, 1, 2, 3};
    assert(!whale_band_decode_indices(reversed_offsets, sizeof(reversed_offsets), WIDTH, 1, 0, scratch, &workspace));
    assert(*whale_band_cached_row(bands, sizeof(bands), WIDTH, HEIGHT, 15, &workspace) == 1);
    assert(*whale_band_cached_row(bands, sizeof(bands), WIDTH, HEIGHT, 16, &workspace) == 2);
    assert(!whale_band_cached_row(bands, sizeof(bands), WIDTH, HEIGHT, 17, &workspace));

    bmp_sink_t sink = {0};
    assert(whale_background_write_bmp(asset, sizeof(asset), WIDTH, HEIGHT, scratch, &workspace, check_bmp_chunk, &sink));
    assert(sink.chunks == 3 && sink.bytes == WHALE_BG_BMP_HEADER_BYTES + WIDTH * HEIGHT);
    sink = (bmp_sink_t){.fail_at = 2};
    assert(!whale_background_write_bmp(asset, sizeof(asset), WIDTH, HEIGHT, scratch, &workspace, check_bmp_chunk, &sink));
    assert(sink.chunks == 2 && sink.bytes == WHALE_BG_BMP_HEADER_BYTES);
}

int main(void) {
    check_deflate_blocks();
    check_bands_and_bmp();
    return 0;
}
