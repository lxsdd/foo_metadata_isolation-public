#include "stdafx.h"
#include "apev2_cuesheet_patch.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace metadata_isolation {
namespace {

using bytes = std::vector<uint8_t>;

constexpr size_t k_ape_record_size = 32;
constexpr size_t k_max_tag_bytes = 64u * 1024u * 1024u;
constexpr uint32_t k_ape_version = 2000;
constexpr uint32_t k_header_present = 0x80000000u;
constexpr uint32_t k_is_header = 0x20000000u;

[[noreturn]] void unsafe(const char* message) {
    throw pfc::exception(message);
}

uint32_t decode_le32(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8) |
           (static_cast<uint32_t>(value[2]) << 16) |
           (static_cast<uint32_t>(value[3]) << 24);
}

void encode_le32(uint32_t value, uint8_t* output) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value >> 16);
    output[3] = static_cast<uint8_t>(value >> 24);
}

bool is_signature(const uint8_t* value) {
    static constexpr char signature[] = "APETAGEX";
    return std::memcmp(value, signature, sizeof(signature) - 1) == 0;
}

bool is_cuesheet_key(const uint8_t* value, size_t size) {
    static constexpr char expected[] = "CUESHEET";
    if (size != sizeof(expected) - 1) return false;
    for (size_t index = 0; index < size; ++index) {
        uint8_t actual = value[index];
        if (actual >= 'a' && actual <= 'z') actual = static_cast<uint8_t>(actual - 'a' + 'A');
        if (actual != static_cast<uint8_t>(expected[index])) return false;
    }
    return true;
}

void validate_utf8(const uint8_t* value, size_t size, const char* message) {
    if (std::find(value, value + size, 0) != value + size) unsafe(message);
    if (size == 0) return;
    const int input_size = pfc::downcast_guarded<int>(size);
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            reinterpret_cast<const char*>(value), input_size,
            nullptr, 0) <= 0) {
        unsafe(message);
    }
}

struct ape_layout {
    bytes extent;
    t_filesize extent_start = 0;
    uint32_t stored_size = 0;
    bool header_present = false;
    size_t footer_offset = 0;
    size_t item_length_offset = 0;
    size_t target_value_begin = 0;
    size_t target_value_end = 0;
};

ape_layout inspect(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort) {
    if (source_path == nullptr || expected_cuesheet == nullptr) {
        unsafe("Missing WavPack APEv2 CUESHEET validation input");
    }
    file::ptr input;
    filesystem::g_open_read(input, source_path, abort);
    const t_filesize file_size = input->get_size(abort);
    if (file_size < k_ape_record_size) unsafe("WavPack source has no trailing APEv2 footer");
    input->seek(file_size - k_ape_record_size, abort);
    uint8_t footer[k_ape_record_size] = {};
    input->read_object(footer, sizeof(footer), abort);
    if (!is_signature(footer)) unsafe("WavPack source has no trailing APEv2 footer");
    const uint32_t version = decode_le32(footer + 8);
    const uint32_t stored_size = decode_le32(footer + 12);
    const uint32_t item_count = decode_le32(footer + 16);
    const uint32_t footer_flags = decode_le32(footer + 20);
    if (version != k_ape_version) unsafe("Only APEv2.0 tags are safely patchable");
    if (stored_size < k_ape_record_size || stored_size > k_max_tag_bytes || stored_size > file_size) {
        unsafe("Invalid or oversized trailing APEv2 tag");
    }
    if (item_count > (stored_size - k_ape_record_size) / 9) {
        unsafe("APEv2 item count exceeds its payload");
    }
    if ((footer_flags & ~k_header_present) != 0 ||
        std::any_of(footer + 24, footer + 32, [](uint8_t value) { return value != 0; })) {
        unsafe("Unsupported APEv2 footer flags or reserved bytes");
    }

    ape_layout output;
    output.stored_size = stored_size;
    output.header_present = (footer_flags & k_header_present) != 0;
    const t_filesize items_start = file_size - stored_size;
    output.extent_start = items_start;
    if (output.header_present) {
        if (items_start < k_ape_record_size) unsafe("APEv2 footer declares a missing header");
        output.extent_start -= k_ape_record_size;
    }
    const t_filesize extent_size_64 = file_size - output.extent_start;
    if (extent_size_64 > k_max_tag_bytes + k_ape_record_size) {
        unsafe("APEv2 extent exceeds the safety limit");
    }
    const size_t extent_size = pfc::downcast_guarded<size_t>(extent_size_64);
    output.extent.resize(extent_size);
    input->seek(output.extent_start, abort);
    input->read_object(output.extent.data(), output.extent.size(), abort);
    input.release();

    const size_t items_offset = output.header_present ? k_ape_record_size : 0;
    output.footer_offset = output.extent.size() - k_ape_record_size;
    if (output.header_present) {
        const uint8_t* header = output.extent.data();
        if (!is_signature(header) || decode_le32(header + 8) != version ||
            decode_le32(header + 12) != stored_size || decode_le32(header + 16) != item_count ||
            decode_le32(header + 20) != (footer_flags | k_is_header) ||
            std::any_of(header + 24, header + 32, [](uint8_t value) { return value != 0; })) {
            unsafe("APEv2 header and footer disagree");
        }
    }

    size_t cursor = items_offset;
    size_t target_count = 0;
    for (uint32_t index = 0; index < item_count; ++index) {
        if (cursor > output.footer_offset || output.footer_offset - cursor < 8) {
            unsafe("Truncated APEv2 item header");
        }
        const size_t length_offset = cursor;
        const uint32_t value_size = decode_le32(output.extent.data() + cursor);
        const uint32_t item_flags = decode_le32(output.extent.data() + cursor + 4);
        cursor += 8;
        const auto key_end = std::find(
            output.extent.begin() + cursor,
            output.extent.begin() + output.footer_offset,
            static_cast<uint8_t>(0));
        if (key_end == output.extent.begin() + output.footer_offset) {
            unsafe("Unterminated APEv2 item key");
        }
        const size_t key_end_offset = static_cast<size_t>(key_end - output.extent.begin());
        if (key_end_offset == cursor || key_end_offset - cursor > 255) {
            unsafe("Invalid APEv2 item key length");
        }
        for (size_t offset = cursor; offset < key_end_offset; ++offset) {
            if (output.extent[offset] < 0x20 || output.extent[offset] > 0x7e) {
                unsafe("Invalid non-ASCII APEv2 item key");
            }
        }
        const bool target = is_cuesheet_key(
            output.extent.data() + cursor, key_end_offset - cursor);
        cursor = key_end_offset + 1;
        if (value_size > output.footer_offset - cursor) unsafe("Truncated APEv2 item value");
        const size_t value_end = cursor + value_size;
        if (target) {
            ++target_count;
            if ((item_flags & ~7u) != 0) {
                unsafe("WavPack CUESHEET APEv2 item has unsupported flags");
            }
            if ((item_flags & 1u) != 0) {
                unsafe("WavPack CUESHEET APEv2 item is read-only");
            }
            if (((item_flags >> 1) & 3u) != 0) {
                unsafe("WavPack CUESHEET APEv2 item is not a text value");
            }
            output.item_length_offset = length_offset;
            output.target_value_begin = cursor;
            output.target_value_end = value_end;
        }
        cursor = value_end;
    }
    if (cursor != output.footer_offset) unsafe("Trailing bytes inside APEv2 item payload");
    if (target_count != 1) unsafe("WavPack must contain exactly one CUESHEET APEv2 item");

    const size_t raw_size = output.target_value_end - output.target_value_begin;
    validate_utf8(
        output.extent.data() + output.target_value_begin,
        raw_size,
        "WavPack CUESHEET item is not valid NUL-free UTF-8");
    if (raw_size != std::strlen(expected_cuesheet) ||
        std::memcmp(
            output.extent.data() + output.target_value_begin,
            expected_cuesheet,
            raw_size) != 0) {
        unsafe("Raw WavPack CUESHEET does not match the prepared foobar2000 snapshot");
    }
    return output;
}

void copy_prefix(
    const file::ptr& input,
    const file::ptr& output,
    t_filesize count,
    abort_callback& abort) {
    std::vector<uint8_t> buffer(1024 * 1024);
    while (count != 0) {
        const size_t chunk = static_cast<size_t>(std::min<t_filesize>(count, buffer.size()));
        input->read_object(buffer.data(), chunk, abort);
        output->write_object(buffer.data(), chunk, abort);
        count -= chunk;
    }
}

} // namespace

void validate_raw_wavpack_apev2_cuesheet(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort) {
    (void)inspect(source_path, expected_cuesheet, abort);
}

void write_raw_wavpack_apev2_cuesheet_patch(
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort) {
    if (replacement == nullptr || expected_before == nullptr ||
        std::strcmp(expected_before, replacement) == 0) {
        unsafe("Raw WavPack CUESHEET replacement is missing or unchanged");
    }
    const size_t replacement_size = std::strlen(replacement);
    if (replacement_size > UINT32_MAX) unsafe("WavPack CUESHEET item is too large");
    validate_utf8(
        reinterpret_cast<const uint8_t*>(replacement),
        replacement_size,
        "WavPack CUESHEET replacement is not valid NUL-free UTF-8");
    const ape_layout layout = inspect(source_path, expected_before, abort);
    const size_t old_size = layout.target_value_end - layout.target_value_begin;
    const uint64_t new_stored_size_64 =
        static_cast<uint64_t>(layout.stored_size) - old_size + replacement_size;
    if (new_stored_size_64 > k_max_tag_bytes || new_stored_size_64 > UINT32_MAX) {
        unsafe("Patched WavPack APEv2 tag exceeds the safety limit");
    }
    const uint32_t new_stored_size = static_cast<uint32_t>(new_stored_size_64);

    bytes patched;
    patched.reserve(layout.extent.size() - old_size + replacement_size);
    patched.insert(
        patched.end(), layout.extent.begin(),
        layout.extent.begin() + layout.target_value_begin);
    patched.insert(patched.end(), replacement, replacement + replacement_size);
    patched.insert(
        patched.end(), layout.extent.begin() + layout.target_value_end,
        layout.extent.end());
    encode_le32(
        pfc::downcast_guarded<uint32_t>(replacement_size),
        patched.data() + layout.item_length_offset);
    const size_t new_footer_offset = patched.size() - k_ape_record_size;
    encode_le32(new_stored_size, patched.data() + new_footer_offset + 12);
    if (layout.header_present) encode_le32(new_stored_size, patched.data() + 12);

    file::ptr input;
    filesystem::g_open_read(input, source_path, abort);
    file::ptr output;
    filesystem::g_open_write_new(output, output_path, abort);
    copy_prefix(input, output, layout.extent_start, abort);
    output->write_object(patched.data(), patched.size(), abort);
    output.release();
    input.release();
}

} // namespace metadata_isolation
