#include "stdafx.h"
#include "flac_cuesheet_patch.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace metadata_isolation {
namespace {

using bytes = std::vector<uint8_t>;

constexpr size_t k_max_metadata_bytes = 256u * 1024u * 1024u;

[[noreturn]] void unsafe(const char* message) {
    throw pfc::exception(message);
}

uint32_t decode_be24(const uint8_t* value) {
    return (static_cast<uint32_t>(value[0]) << 16) |
           (static_cast<uint32_t>(value[1]) << 8) |
           static_cast<uint32_t>(value[2]);
}

void encode_be24(uint32_t value, uint8_t* output) {
    if (value >= (1u << 24)) unsafe("FLAC metadata block exceeds its 24-bit size limit");
    output[0] = static_cast<uint8_t>(value >> 16);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value);
}

uint32_t decode_le32(const bytes& input, size_t offset, size_t end) {
    if (offset > end || end - offset < 4) unsafe("Truncated FLAC Vorbis Comment length");
    return static_cast<uint32_t>(input[offset]) |
           (static_cast<uint32_t>(input[offset + 1]) << 8) |
           (static_cast<uint32_t>(input[offset + 2]) << 16) |
           (static_cast<uint32_t>(input[offset + 3]) << 24);
}

void encode_le32(uint32_t value, uint8_t* output) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value >> 16);
    output[3] = static_cast<uint8_t>(value >> 24);
}

size_t checked_end(size_t begin, size_t count, size_t limit, const char* message) {
    if (begin > limit || count > limit - begin) unsafe(message);
    return begin + count;
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

struct flac_layout {
    bytes metadata;
    t_filesize audio_offset = 0;
    size_t comment_header = 0;
    size_t comment_payload_begin = 0;
    size_t comment_payload_end = 0;
    size_t target_length = 0;
    size_t target_data_begin = 0;
    size_t target_value_begin = 0;
    size_t target_data_end = 0;
};

flac_layout inspect(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort) {
    if (source_path == nullptr || expected_cuesheet == nullptr) {
        unsafe("Missing FLAC CUESHEET validation input");
    }

    file::ptr input;
    filesystem::g_open_read(input, source_path, abort);
    flac_layout output;
    uint8_t marker[4] = {};
    input->read_object(marker, sizeof(marker), abort);
    if (std::memcmp(marker, "fLaC", sizeof(marker)) != 0) {
        unsafe("Physical file does not begin with the native FLAC marker");
    }
    output.metadata.insert(output.metadata.end(), marker, marker + sizeof(marker));

    size_t block_index = 0;
    size_t comment_count = 0;
    for (;;) {
        uint8_t header[4] = {};
        input->read_object(header, sizeof(header), abort);
        const bool last = (header[0] & 0x80) != 0;
        const unsigned type = header[0] & 0x7f;
        const size_t payload_size = decode_be24(header + 1);
        if (type == 0x7f) unsafe("FLAC contains an invalid metadata block type");
        const size_t header_offset = output.metadata.size();
        if (output.metadata.size() > k_max_metadata_bytes - sizeof(header) ||
            payload_size > k_max_metadata_bytes - output.metadata.size() - sizeof(header)) {
            unsafe("FLAC metadata exceeds the safety limit");
        }
        output.metadata.insert(output.metadata.end(), header, header + sizeof(header));
        const size_t payload_begin = output.metadata.size();
        output.metadata.resize(payload_begin + payload_size);
        if (payload_size != 0) {
            input->read_object(output.metadata.data() + payload_begin, payload_size, abort);
        }
        const size_t payload_end = output.metadata.size();

        if (block_index == 0 && (type != 0 || payload_size != 34)) {
            unsafe("FLAC STREAMINFO must be the first metadata block with size 34");
        }
        if (block_index != 0 && type == 0) unsafe("FLAC contains a duplicate STREAMINFO block");
        if (type == 4) {
            ++comment_count;
            output.comment_header = header_offset;
            output.comment_payload_begin = payload_begin;
            output.comment_payload_end = payload_end;
        }
        if (type == 5) {
            unsafe("Native FLAC CUESHEET blocks are not supported by textual-CUESHEET transactions");
        }
        ++block_index;
        if (last) break;
    }
    if (comment_count != 1) unsafe("FLAC must contain exactly one Vorbis Comment metadata block");
    output.audio_offset = input->get_position(abort);
    input.release();

    size_t cursor = output.comment_payload_begin;
    const size_t end = output.comment_payload_end;
    const size_t vendor_size = decode_le32(output.metadata, cursor, end);
    cursor = checked_end(cursor + 4, vendor_size, end, "Truncated FLAC Vorbis Comment vendor");
    const uint32_t field_count = decode_le32(output.metadata, cursor, end);
    cursor += 4;
    if (field_count > (end - cursor) / 4) {
        unsafe("FLAC Vorbis Comment field count exceeds its payload");
    }
    size_t target_count = 0;
    for (uint32_t index = 0; index < field_count; ++index) {
        const size_t length_offset = cursor;
        const size_t field_size = decode_le32(output.metadata, cursor, end);
        const size_t field_begin = cursor + 4;
        const size_t field_end = checked_end(
            field_begin, field_size, end, "Truncated FLAC Vorbis Comment field");
        const auto equals = std::find(
            output.metadata.begin() + field_begin,
            output.metadata.begin() + field_end,
            static_cast<uint8_t>('='));
        if (equals != output.metadata.begin() + field_end) {
            const size_t equals_offset = static_cast<size_t>(equals - output.metadata.begin());
            if (is_cuesheet_key(
                    output.metadata.data() + field_begin,
                    equals_offset - field_begin)) {
                ++target_count;
                output.target_length = length_offset;
                output.target_data_begin = field_begin;
                output.target_value_begin = equals_offset + 1;
                output.target_data_end = field_end;
            }
        }
        cursor = field_end;
    }
    if (cursor != end) unsafe("FLAC Vorbis Comment block has trailing bytes");
    if (target_count != 1) unsafe("FLAC must contain exactly one CUESHEET Vorbis Comment entry");

    const size_t raw_size = output.target_data_end - output.target_value_begin;
    validate_utf8(
        output.metadata.data() + output.target_value_begin,
        raw_size,
        "FLAC CUESHEET comment is not valid NUL-free UTF-8");
    if (raw_size != std::strlen(expected_cuesheet) ||
        std::memcmp(
            output.metadata.data() + output.target_value_begin,
            expected_cuesheet,
            raw_size) != 0) {
        unsafe("Raw FLAC CUESHEET does not match the prepared foobar2000 snapshot");
    }
    return output;
}

void copy_remainder(const file::ptr& input, const file::ptr& output, abort_callback& abort) {
    std::vector<uint8_t> buffer(1024 * 1024);
    for (;;) {
        const size_t count = input->read(buffer.data(), buffer.size(), abort);
        if (count == 0) break;
        output->write_object(buffer.data(), count, abort);
    }
}

} // namespace

void validate_raw_flac_vorbis_cuesheet(
    const char* source_path,
    const char* expected_cuesheet,
    abort_callback& abort) {
    (void)inspect(source_path, expected_cuesheet, abort);
}

void write_raw_flac_vorbis_cuesheet_patch(
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort) {
    if (replacement == nullptr || expected_before == nullptr ||
        std::strcmp(expected_before, replacement) == 0) {
        unsafe("Raw FLAC CUESHEET replacement is missing or unchanged");
    }
    const size_t replacement_size = std::strlen(replacement);
    validate_utf8(
        reinterpret_cast<const uint8_t*>(replacement),
        replacement_size,
        "FLAC CUESHEET replacement is not valid NUL-free UTF-8");
    const flac_layout layout = inspect(source_path, expected_before, abort);
    const size_t key_size = layout.target_value_begin - layout.target_data_begin;
    if (replacement_size > UINT32_MAX - key_size) unsafe("FLAC CUESHEET comment is too large");
    const size_t new_field_size = key_size + replacement_size;
    const size_t old_field_size = layout.target_data_end - layout.target_data_begin;
    const size_t old_comment_size = layout.comment_payload_end - layout.comment_payload_begin;
    if (new_field_size > old_field_size + ((1u << 24) - 1u - old_comment_size)) {
        unsafe("Patched FLAC Vorbis Comment block exceeds its 24-bit size limit");
    }
    const size_t new_comment_size = old_comment_size - old_field_size + new_field_size;

    bytes patched;
    patched.reserve(layout.metadata.size() - old_field_size + new_field_size);
    patched.insert(
        patched.end(), layout.metadata.begin(), layout.metadata.begin() + layout.target_length);
    uint8_t field_length[4] = {};
    encode_le32(pfc::downcast_guarded<uint32_t>(new_field_size), field_length);
    patched.insert(patched.end(), field_length, field_length + sizeof(field_length));
    patched.insert(
        patched.end(),
        layout.metadata.begin() + layout.target_data_begin,
        layout.metadata.begin() + layout.target_value_begin);
    patched.insert(patched.end(), replacement, replacement + replacement_size);
    patched.insert(
        patched.end(),
        layout.metadata.begin() + layout.target_data_end,
        layout.metadata.end());
    encode_be24(
        pfc::downcast_guarded<uint32_t>(new_comment_size),
        patched.data() + layout.comment_header + 1);

    file::ptr input;
    filesystem::g_open_read(input, source_path, abort);
    input->seek(layout.audio_offset, abort);
    file::ptr output;
    filesystem::g_open_write_new(output, output_path, abort);
    output->write_object(patched.data(), patched.size(), abort);
    copy_remainder(input, output, abort);
    output.release();
    input.release();
}

} // namespace metadata_isolation
