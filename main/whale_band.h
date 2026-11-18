#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WHALE_BAND_ROWS 16
#define WHALE_BAND_MAX_WIDTH 240
#define WHALE_BAND_MAX_HEIGHT 320
#define WHALE_BG_PALETTE_BYTES (256 * 3)
#define WHALE_BG_BMP_HEADER_BYTES (54 + 256 * 4)

/* Reusable scratch space for one synchronous inflate. */
typedef struct {
    uint16_t literal_symbols[288];
    uint16_t distance_symbols[32];
    uint16_t code_symbols[19];
    uint8_t lengths[288 + 32];
} whale_band_workspace_t;

/* Inflate one raw RFC 1951 stream into dst without heap allocations. */
bool whale_band_inflate(const uint8_t *src, size_t src_len, uint8_t *dst, size_t expected_len,
                        whale_band_workspace_t *workspace);

/* A band payload starts with little-endian uint16 offsets (one per band,
 * plus the end offset), relative to the following raw Deflate streams.
 * out_indices has room for min(16, remaining rows) * width bytes. */
bool whale_band_decode_indices(const uint8_t *band_payload, size_t payload_bytes,
                               unsigned width, unsigned height, unsigned band_index,
                               uint8_t *out_indices, whale_band_workspace_t *workspace);

/* Synchronous LVGL draw path only. Payloads are immutable flash data. */
const uint8_t *whale_band_cached_row(const uint8_t *band_payload, size_t payload_bytes,
                                     unsigned width, unsigned height, unsigned y,
                                     whale_band_workspace_t *workspace);

typedef bool (*whale_bmp_write_fn)(void *context, const uint8_t *data, size_t length);

/* Stream an indexed preset as an 8-bit BMP. The sink consumes each chunk
 * synchronously. The caller owns a separate WHALE_BAND_ROWS * width buffer. */
bool whale_background_write_bmp(const uint8_t *data, size_t length,
                                unsigned width, unsigned height, uint8_t *scratch,
                                whale_band_workspace_t *workspace,
                                whale_bmp_write_fn write, void *context);
