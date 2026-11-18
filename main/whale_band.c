#include "whale_band.h"
#include <string.h>

typedef struct {
    const uint8_t *src;
    size_t len;
    size_t pos;
    uint32_t bit_buf;
    unsigned bit_count;
} band_bits_t;

typedef struct {
    uint16_t count[16];
    uint16_t *symbol;
} band_huff_t;

static unsigned read_le16(const uint8_t *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
}

static bool read_bits(band_bits_t *bs, unsigned n, uint32_t *out) {
    if (n == 0) {
        *out = 0;
        return true;
    }
    while (bs->bit_count < n) {
        if (bs->pos >= bs->len) return false;
        bs->bit_buf |= (uint32_t)bs->src[bs->pos++] << bs->bit_count;
        bs->bit_count += 8;
    }
    *out = bs->bit_buf & ((1u << n) - 1u);
    bs->bit_buf >>= n;
    bs->bit_count -= n;
    return true;
}

static bool build_huff(band_huff_t *h, const uint8_t *lengths, unsigned n, bool allow_single_or_empty) {
    memset(h->count, 0, sizeof(h->count));
    for (unsigned i = 0; i < n; i++) {
        if (lengths[i] > 15) return false;
        h->count[lengths[i]]++;
    }
    if (h->count[0] == n) return allow_single_or_empty;

    int left = 1;
    for (unsigned len = 1; len <= 15; len++) {
        left <<= 1;
        left -= (int)h->count[len];
        if (left < 0) return false;
    }
    if (left && !(allow_single_or_empty && h->count[0] == n - 1 && h->count[1] == 1)) return false;

    uint16_t offs[16];
    offs[1] = 0;
    for (unsigned len = 1; len < 15; len++) {
        offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
    }
    for (unsigned sym = 0; sym < n; sym++) {
        if (lengths[sym] != 0) {
            h->symbol[offs[lengths[sym]]++] = (uint16_t)sym;
        }
    }
    return true;
}

static bool decode_sym(band_bits_t *bs, const band_huff_t *h, unsigned *sym_out) {
    uint32_t bits = bs->bit_buf;
    unsigned available = bs->bit_count;
    size_t pos = bs->pos;
    int code = 0, first = 0, index = 0;
    for (unsigned len = 1; len <= 15; len++) {
        if (!available) {
            if (pos >= bs->len) return false;
            bits = bs->src[pos++];
            available = 8;
        }
        code |= (int)(bits & 1u);
        bits >>= 1;
        available--;
        int count = (int)h->count[len];
        if (code - count < first) {
            bs->bit_buf = bits;
            bs->bit_count = available;
            bs->pos = pos;
            *sym_out = h->symbol[index + (code - first)];
            return true;
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return false;
}

static bool inflate_stored(band_bits_t *bs, uint8_t *dst, size_t expected_len, size_t *out_pos) {
    /* Rewind whole unread bytes buffered in bit_buf before reading byte-aligned LEN/NLEN. */
    bs->pos -= bs->bit_count / 8u;
    bs->bit_buf = 0;
    bs->bit_count = 0;
    if (bs->len - bs->pos < 4) return false;
    unsigned len = read_le16(bs->src + bs->pos);
    unsigned nlen = read_le16(bs->src + bs->pos + 2);
    bs->pos += 4;
    if ((len ^ 0xFFFFu) != nlen) return false;
    if (len > bs->len - bs->pos || len > expected_len - *out_pos) return false;
    if (len > 0) {
        memcpy(dst + *out_pos, bs->src + bs->pos, len);
        bs->pos += len;
        *out_pos += len;
    }
    return true;
}

static void build_fixed_trees(band_huff_t *lit, band_huff_t *dist, uint8_t *lengths) {
    for (unsigned i = 0; i < 144; i++) lengths[i] = 8;
    for (unsigned i = 144; i < 256; i++) lengths[i] = 9;
    for (unsigned i = 256; i < 280; i++) lengths[i] = 7;
    for (unsigned i = 280; i < 288; i++) lengths[i] = 8;
    (void)build_huff(lit, lengths, 288, false);
    memset(lengths, 5, 32);
    (void)build_huff(dist, lengths, 32, false);
}

static bool build_dynamic_trees(band_bits_t *bs, band_huff_t *lit, band_huff_t *dist,
                                whale_band_workspace_t *workspace) {
    static const uint8_t cl_order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    uint32_t hlit_v = 0, hdist_v = 0, hclen_v = 0;
    if (!read_bits(bs, 5, &hlit_v) || !read_bits(bs, 5, &hdist_v) || !read_bits(bs, 4, &hclen_v)) {
        return false;
    }
    unsigned nlit = (unsigned)hlit_v + 257u;
    unsigned ndist = (unsigned)hdist_v + 1u;
    unsigned ncode = (unsigned)hclen_v + 4u;
    if (nlit > 286u || ndist > 32u) return false;

    uint8_t cl_lens[19];
    memset(cl_lens, 0, sizeof(cl_lens));
    for (unsigned i = 0; i < ncode; i++) {
        uint32_t v = 0;
        if (!read_bits(bs, 3, &v)) return false;
        cl_lens[cl_order[i]] = (uint8_t)v;
    }
    band_huff_t cl_tree = {.symbol = workspace->code_symbols};
    if (!build_huff(&cl_tree, cl_lens, 19, false)) return false;

    uint8_t *lengths = workspace->lengths;
    unsigned total = nlit + ndist;
    unsigned i = 0;
    while (i < total) {
        unsigned sym = 0;
        if (!decode_sym(bs, &cl_tree, &sym)) return false;
        if (sym < 16u) {
            lengths[i++] = (uint8_t)sym;
            continue;
        }
        unsigned rep = 0;
        uint8_t val = 0;
        uint32_t extra = 0;
        if (sym == 16u) {
            if (i == 0 || !read_bits(bs, 2, &extra)) return false;
            val = lengths[i - 1];
            rep = 3u + (unsigned)extra;
        } else if (sym == 17u) {
            if (!read_bits(bs, 3, &extra)) return false;
            rep = 3u + (unsigned)extra;
        } else {
            if (!read_bits(bs, 7, &extra)) return false;
            rep = 11u + (unsigned)extra;
        }
        if (i + rep > total) return false;
        memset(lengths + i, val, rep);
        i += rep;
    }
    if (lengths[256] == 0) return false;
    return build_huff(lit, lengths, nlit, true) && build_huff(dist, lengths + nlit, ndist, true);
}

static bool inflate_codes(band_bits_t *bs, const band_huff_t *lit, const band_huff_t *dist,
                          uint8_t *dst, size_t expected_len, size_t *out_pos) {
    static const uint16_t len_base[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    static const uint8_t len_extra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static const uint16_t dist_base[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577
    };
    static const uint8_t dist_extra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };

    for (;;) {
        unsigned sym = 0;
        if (!decode_sym(bs, lit, &sym)) return false;
        if (sym < 256u) {
            if (*out_pos >= expected_len) return false;
            dst[(*out_pos)++] = (uint8_t)sym;
            continue;
        }
        if (sym == 256u) return true;
        if (sym > 285u) return false;

        unsigned len_idx = sym - 257u;
        uint32_t extra = 0;
        if (!read_bits(bs, len_extra[len_idx], &extra)) return false;
        unsigned copy_len = (unsigned)len_base[len_idx] + (unsigned)extra;

        unsigned dist_sym = 0;
        if (!decode_sym(bs, dist, &dist_sym) || dist_sym >= 30u) return false;
        if (!read_bits(bs, dist_extra[dist_sym], &extra)) return false;
        unsigned dist_val = (unsigned)dist_base[dist_sym] + (unsigned)extra;

        if (dist_val == 0u || dist_val > *out_pos || copy_len > expected_len - *out_pos) {
            return false;
        }
        /* Copy byte-by-byte forward because LZ77 matches can overlap when dist_val < copy_len. */
        size_t pos = *out_pos;
        for (unsigned k = 0; k < copy_len; k++) {
            dst[pos + k] = dst[pos - dist_val + k];
        }
        *out_pos = pos + copy_len;
    }
}

bool whale_band_inflate(const uint8_t *src, size_t src_len, uint8_t *dst, size_t expected_len,
                        whale_band_workspace_t *workspace) {
    if (!src || (!dst && expected_len) || !workspace) return false;
    band_bits_t bs = {.src = src, .len = src_len};
    size_t out_pos = 0;
    band_huff_t lit = {.symbol = workspace->literal_symbols};
    band_huff_t dist = {.symbol = workspace->distance_symbols};

    for (;;) {
        uint32_t bfinal = 0, btype = 0;
        if (!read_bits(&bs, 1, &bfinal) || !read_bits(&bs, 2, &btype)) return false;
        bool ok = false;
        if (btype == 0u) {
            ok = inflate_stored(&bs, dst, expected_len, &out_pos);
        } else if (btype == 1u) {
            build_fixed_trees(&lit, &dist, workspace->lengths);
            ok = inflate_codes(&bs, &lit, &dist, dst, expected_len, &out_pos);
        } else if (btype == 2u) {
            ok = build_dynamic_trees(&bs, &lit, &dist, workspace) &&
                 inflate_codes(&bs, &lit, &dist, dst, expected_len, &out_pos);
        }
        if (!ok) return false;
        if (bfinal) break;
    }
    return out_pos == expected_len && bs.pos == bs.len;
}

bool whale_band_decode_indices(const uint8_t *band_payload, size_t payload_bytes,
                               unsigned width, unsigned height, unsigned band_index,
                               uint8_t *out_indices, whale_band_workspace_t *workspace) {
    if (!band_payload || !out_indices || width == 0u || width > WHALE_BAND_MAX_WIDTH ||
        height == 0u || height > WHALE_BAND_MAX_HEIGHT) {
        return false;
    }
    unsigned band_count = (height + WHALE_BAND_ROWS - 1u) / WHALE_BAND_ROWS;
    if (band_index >= band_count) return false;
    size_t table_bytes = (size_t)(band_count + 1u) * 2u;
    if (payload_bytes <= table_bytes) return false;

    size_t data_bytes = payload_bytes - table_bytes;
    if (read_le16(band_payload) != 0 || read_le16(band_payload + table_bytes - 2u) != data_bytes) {
        return false;
    }
    unsigned start = read_le16(band_payload + band_index * 2u);
    unsigned end = read_le16(band_payload + (band_index + 1u) * 2u);
    if (end <= start || end > data_bytes) return false;

    unsigned y_start = band_index * WHALE_BAND_ROWS;
    unsigned rows = height - y_start < WHALE_BAND_ROWS ? height - y_start : WHALE_BAND_ROWS;
    return whale_band_inflate(band_payload + table_bytes + start, end - start,
                              out_indices, (size_t)rows * width, workspace);
}

const uint8_t *whale_band_cached_row(const uint8_t *band_payload, size_t payload_bytes,
                                     unsigned width, unsigned height, unsigned y,
                                     whale_band_workspace_t *workspace) {
    static uint8_t s_cache[WHALE_BAND_ROWS * WHALE_BAND_MAX_WIDTH];
    static const uint8_t *s_payload;
    static unsigned s_band;

    if (!band_payload || y >= height) return NULL;
    unsigned band = y / WHALE_BAND_ROWS;
    if (s_payload != band_payload || s_band != band) {
        if (!whale_band_decode_indices(band_payload, payload_bytes, width, height, band, s_cache, workspace)) {
            s_payload = NULL;
            return NULL;
        }
        s_payload = band_payload;
        s_band = band;
    }
    return s_cache + (size_t)(y % WHALE_BAND_ROWS) * width;
}

static void bmp_header(unsigned width, unsigned height, const uint8_t *bgr_palette,
                       uint8_t *out_header) {
    uint32_t image_bytes = (uint32_t)width * (uint32_t)height;
    memset(out_header, 0, WHALE_BG_BMP_HEADER_BYTES);
    out_header[0] = 'B';
    out_header[1] = 'M';
    write_le32(out_header + 2, (uint32_t)WHALE_BG_BMP_HEADER_BYTES + image_bytes);
    write_le32(out_header + 10, WHALE_BG_BMP_HEADER_BYTES);
    write_le32(out_header + 14, 40u);
    write_le32(out_header + 18, (uint32_t)width);
    write_le32(out_header + 22, (uint32_t)height);
    write_le16(out_header + 26, 1u);
    write_le16(out_header + 28, 8u);
    write_le32(out_header + 34, image_bytes);
    write_le32(out_header + 38, 3780u);
    write_le32(out_header + 42, 3780u);
    write_le32(out_header + 46, 256u);
    write_le32(out_header + 50, 256u);
    uint8_t *pal_out = out_header + 54;
    for (unsigned i = 0; i < 256u; i++) {
        pal_out[i * 4u + 0u] = bgr_palette[i * 3u + 0u];
        pal_out[i * 4u + 1u] = bgr_palette[i * 3u + 1u];
        pal_out[i * 4u + 2u] = bgr_palette[i * 3u + 2u];
        pal_out[i * 4u + 3u] = 0u;
    }
}

bool whale_background_write_bmp(const uint8_t *data, size_t length,
                                unsigned width, unsigned height, uint8_t *scratch,
                                whale_band_workspace_t *workspace,
                                whale_bmp_write_fn write, void *context) {
    if (!data || length <= WHALE_BG_PALETTE_BYTES || !width || width % 4 ||
        width > WHALE_BAND_MAX_WIDTH || !height || height > WHALE_BAND_MAX_HEIGHT ||
        width * WHALE_BAND_ROWS < WHALE_BG_BMP_HEADER_BYTES || !scratch || !workspace || !write) {
        return false;
    }
    bmp_header(width, height, data, scratch);
    if (!write(context, scratch, WHALE_BG_BMP_HEADER_BYTES)) return false;

    unsigned band_count = (height + WHALE_BAND_ROWS - 1u) / WHALE_BAND_ROWS;
    unsigned rows = height - (band_count - 1u) * WHALE_BAND_ROWS;
    for (unsigned band = band_count; band-- > 0;) {
        if (!whale_band_decode_indices(data + WHALE_BG_PALETTE_BYTES,
                length - WHALE_BG_PALETTE_BYTES, width, height, band, scratch, workspace)) {
            return false;
        }
        /* BMP is bottom-up. Reverse each decoded band in place before sending it. */
        for (unsigned y = 0; y < rows / 2; y++) {
            uint8_t *top = scratch + y * width;
            uint8_t *bottom = scratch + (rows - 1 - y) * width;
            for (unsigned x = 0; x < width; x++) {
                uint8_t index = top[x];
                top[x] = bottom[x];
                bottom[x] = index;
            }
        }
        if (!write(context, scratch, rows * width)) return false;
        rows = WHALE_BAND_ROWS;
    }
    return true;
}
