#include "stdafx.h"
#include "id3_cuesheet_patch.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

namespace metadata_isolation {
namespace {

using bytes = std::vector<uint8_t>;

[[noreturn]] void unsafe(const char* message) {
    throw pfc::exception(message);
}

uint32_t decode_synchsafe(const uint8_t* value) {
    if ((value[0] | value[1] | value[2] | value[3]) & 0x80) unsafe("Invalid ID3v2 synchsafe size");
    return (static_cast<uint32_t>(value[0]) << 21) |
           (static_cast<uint32_t>(value[1]) << 14) |
           (static_cast<uint32_t>(value[2]) << 7) |
           static_cast<uint32_t>(value[3]);
}

void encode_synchsafe(uint32_t value, uint8_t* output) {
    if (value >= (1u << 28)) unsafe("ID3v2 tag exceeds the synchsafe size limit");
    output[0] = static_cast<uint8_t>((value >> 21) & 0x7f);
    output[1] = static_cast<uint8_t>((value >> 14) & 0x7f);
    output[2] = static_cast<uint8_t>((value >> 7) & 0x7f);
    output[3] = static_cast<uint8_t>(value & 0x7f);
}

uint32_t decode_be32(const uint8_t* value) {
    return (static_cast<uint32_t>(value[0]) << 24) |
           (static_cast<uint32_t>(value[1]) << 16) |
           (static_cast<uint32_t>(value[2]) << 8) |
           static_cast<uint32_t>(value[3]);
}

void encode_be32(uint32_t value, uint8_t* output) {
    output[0] = static_cast<uint8_t>(value >> 24);
    output[1] = static_cast<uint8_t>(value >> 16);
    output[2] = static_cast<uint8_t>(value >> 8);
    output[3] = static_cast<uint8_t>(value);
}

void encode_be24(uint32_t value, uint8_t* output) {
    if (value >= (1u << 24)) unsafe("ID3v2.2 frame exceeds its 24-bit size limit");
    output[0] = static_cast<uint8_t>(value >> 16);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value);
}

wchar_t decode_windows_1252(uint8_t value) {
    static constexpr wchar_t c1[] = {
        0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
        0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178,
    };
    return value >= 0x80 && value <= 0x9f ? c1[value - 0x80] :
        static_cast<wchar_t>(value);
}

bool encode_windows_1252(wchar_t character, uint8_t& output) {
    if ((character >= 0x0001 && character <= 0x007f) ||
        (character >= 0x00a0 && character <= 0x00ff)) {
        output = static_cast<uint8_t>(character);
        return true;
    }
    static constexpr wchar_t c1[] = {
        0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
        0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178,
    };
    for (size_t index = 0; index < std::size(c1); ++index) {
        if (c1[index] == character) {
            output = static_cast<uint8_t>(0x80 + index);
            return true;
        }
    }
    return false;
}

std::wstring utf8_to_wide(const char* value) {
    if (value == nullptr) unsafe("Missing UTF-8 value");
    const int input_size = pfc::downcast_guarded<int>(std::strlen(value));
    if (input_size == 0) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, input_size, nullptr, 0);
    if (count <= 0) unsafe("Invalid UTF-8 value");
    std::wstring output(static_cast<size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, input_size, output.data(), count) != count) {
        unsafe("Could not decode UTF-8 value");
    }
    return output;
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int input_size = pfc::downcast_guarded<int>(value.size());
    const int count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), input_size, nullptr, 0, nullptr, nullptr);
    if (count <= 0) unsafe("Invalid UTF-16 text in ID3v2 frame");
    std::string output(static_cast<size_t>(count), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), input_size,
            output.data(), count, nullptr, nullptr) != count) {
        unsafe("Could not encode ID3v2 text as UTF-8");
    }
    return output;
}

std::wstring decode_wide(const uint8_t* input, size_t size, bool big_endian) {
    if (size % 2 != 0) unsafe("Odd-length UTF-16 text in ID3v2 frame");
    std::wstring output;
    output.reserve(size / 2);
    for (size_t offset = 0; offset < size; offset += 2) {
        const uint16_t value = big_endian
            ? static_cast<uint16_t>((input[offset] << 8) | input[offset + 1])
            : static_cast<uint16_t>(input[offset] | (input[offset + 1] << 8));
        if (value == 0) unsafe("Embedded NUL in ID3v2 text");
        output.push_back(static_cast<wchar_t>(value));
    }
    return output;
}

std::string decode_text(unsigned encoding, const bytes& raw) {
    if (encoding == 0) {
        std::wstring wide;
        wide.reserve(raw.size());
        for (uint8_t value : raw) {
            if (value == 0) unsafe("Embedded NUL in ID3v2 text");
            wide.push_back(decode_windows_1252(value));
        }
        return wide_to_utf8(wide);
    }
    if (encoding == 3) {
        if (std::find(raw.begin(), raw.end(), 0) != raw.end()) unsafe("Embedded NUL in ID3v2 text");
        std::string value(raw.begin(), raw.end());
        const std::wstring validated = utf8_to_wide(value.c_str());
        if (validated.empty() && !value.empty()) unsafe("Invalid UTF-8 text in ID3v2 frame");
        return value;
    }
    if (encoding == 2) return wide_to_utf8(decode_wide(raw.data(), raw.size(), true));
    if (encoding == 1) {
        if (raw.size() < 2) unsafe("UTF-16 ID3v2 text is missing its byte-order mark");
        if (raw[0] == 0xff && raw[1] == 0xfe) {
            return wide_to_utf8(decode_wide(raw.data() + 2, raw.size() - 2, false));
        }
        if (raw[0] == 0xfe && raw[1] == 0xff) {
            return wide_to_utf8(decode_wide(raw.data() + 2, raw.size() - 2, true));
        }
        unsafe("UTF-16 ID3v2 text is missing its byte-order mark");
    }
    unsafe("Unsupported ID3v2 text encoding");
}

bytes encode_wide(const std::wstring& value, bool big_endian) {
    bytes output;
    output.reserve(value.size() * 2);
    for (wchar_t character : value) {
        const uint16_t unit = static_cast<uint16_t>(character);
        if (unit == 0) unsafe("Replacement contains NUL");
        if (big_endian) {
            output.push_back(static_cast<uint8_t>(unit >> 8));
            output.push_back(static_cast<uint8_t>(unit));
        } else {
            output.push_back(static_cast<uint8_t>(unit));
            output.push_back(static_cast<uint8_t>(unit >> 8));
        }
    }
    return output;
}

bytes encode_text(unsigned encoding, const bytes& old_raw, const bytes& description, const char* replacement) {
    const std::wstring wide = utf8_to_wide(replacement);
    if (encoding == 3) return bytes(replacement, replacement + std::strlen(replacement));
    if (encoding == 0) {
        bytes output;
        output.reserve(wide.size());
        for (wchar_t character : wide) {
            uint8_t encoded = 0;
            if (!encode_windows_1252(character, encoded)) {
                unsafe("Replacement is not representable in the original ID3 single-byte (Windows-1252) frame encoding");
            }
            output.push_back(encoded);
        }
        return output;
    }
    if (encoding == 2) return encode_wide(wide, true);
    if (encoding == 1) {
        bool big_endian = false;
        if (old_raw.size() >= 2 && old_raw[0] == 0xff && old_raw[1] == 0xfe) {
            big_endian = false;
        } else if (old_raw.size() >= 2 && old_raw[0] == 0xfe && old_raw[1] == 0xff) {
            big_endian = true;
        } else if (description.size() >= 2 && description[0] == 0xff && description[1] == 0xfe) {
            big_endian = false;
        } else if (description.size() >= 2 && description[0] == 0xfe && description[1] == 0xff) {
            big_endian = true;
        } else {
            unsafe("UTF-16 replacement byte order is ambiguous");
        }
        bytes output = big_endian ? bytes{0xfe, 0xff} : bytes{0xff, 0xfe};
        bytes encoded = encode_wide(wide, big_endian);
        output.insert(output.end(), encoded.begin(), encoded.end());
        return output;
    }
    unsafe("Unsupported ID3v2 text encoding");
}

size_t find_separator(const bytes& input, size_t begin, size_t end, size_t width) {
    for (size_t offset = begin; offset + width <= end; offset += width) {
        bool zero = true;
        for (size_t index = 0; index < width; ++index) zero = zero && input[offset + index] == 0;
        if (zero) return offset;
    }
    return SIZE_MAX;
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

id3_cuesheet_patch_result write_raw_id3v2_cuesheet_patch(
    const char* source_path,
    const char* output_path,
    const char* expected_before,
    const char* replacement,
    abort_callback& abort) {
    if (expected_before == nullptr || replacement == nullptr || std::strcmp(expected_before, replacement) == 0) {
        unsafe("Raw CUESHEET replacement is missing or unchanged");
    }

    file::ptr input;
    filesystem::g_open_read(input, source_path, abort);
    uint8_t header[10] = {};
    input->read_object(header, sizeof(header), abort);
    if (std::memcmp(header, "ID3", 3) != 0) unsafe("Physical file does not begin with an ID3v2 tag");
    const unsigned major = header[3];
    const uint8_t flags = header[5];
    if (major != 2 && major != 3 && major != 4) unsafe("Only plain ID3v2.2, ID3v2.3 and ID3v2.4 tags are safely patchable");
    if ((flags & 0x80) != 0) unsafe("Tag-level ID3v2 unsynchronisation is not safely patchable");
    if (major == 2 && (flags & 0x40) != 0) unsafe("ID3v2.2 tag compression is not safely patchable");
    if (major == 2 && (flags & 0x3f) != 0) unsafe("Invalid ID3v2.2 header flags");
    if ((major == 3 && (flags & 0x1f) != 0) || (major == 4 && (flags & 0x0f) != 0)) {
        unsafe("Invalid ID3v2 header flags");
    }

    const size_t body_size = decode_synchsafe(header + 6);
    const size_t footer_size = major == 4 && (flags & 0x10) != 0 ? 10 : 0;
    constexpr size_t k_max_tag_bytes = 256u * 1024u * 1024u;
    if (body_size > k_max_tag_bytes - 20 || 10 + body_size + footer_size > input->get_size_ex(abort)) {
        unsafe("ID3v2 tag size is invalid or exceeds the safety limit");
    }
    const size_t old_extent = 10 + body_size + footer_size;
    bytes tag(old_extent);
    std::memcpy(tag.data(), header, sizeof(header));
    input->read_object(tag.data() + 10, old_extent - 10, abort);
    if (footer_size && std::memcmp(tag.data() + 10 + body_size, "3DI", 3) != 0) {
        unsafe("Declared ID3v2 footer is missing");
    }
    if (footer_size && std::memcmp(tag.data() + 10 + body_size + 3, tag.data() + 3, 7) != 0) {
        unsafe("ID3v2 footer does not match its header");
    }

    size_t cursor = 10;
    const size_t body_end = 10 + body_size;
    size_t v23_padding_field = SIZE_MAX;
    if ((flags & 0x40) != 0) {
        if (major == 2) unsafe("ID3v2.2 does not define an extended header");
        if (cursor + 4 > body_end) unsafe("Truncated ID3v2 extended header");
        const size_t extended = major == 3 ? decode_be32(tag.data() + cursor) + 4 : decode_synchsafe(tag.data() + cursor);
        if (extended < 4 || cursor + extended > body_end) unsafe("Invalid ID3v2 extended header size");
        if (major == 3) {
            if (extended != 10 || tag[cursor + 4] != 0 || tag[cursor + 5] != 0) {
                unsafe("CRC or unknown ID3v2.3 extended-header flags are not safely patchable");
            }
            v23_padding_field = cursor + 6;
        } else if (extended != 6 || tag[cursor + 4] != 1 || tag[cursor + 5] != 0) {
            unsafe("Flagged ID3v2.4 extended headers are not safely patchable");
        }
        cursor += extended;
    }

    struct candidate {
        size_t frame_start = 0;
        size_t payload_start = 0;
        size_t payload_end = 0;
        unsigned encoding = 0;
        bytes description;
        bytes old_value;
        bytes value_terminator;
    } target;
    size_t candidate_count = 0;
    size_t padding_start = body_end;

    while (cursor < body_end) {
        if (tag[cursor] == 0) {
            padding_start = cursor;
            if (std::any_of(tag.begin() + cursor, tag.begin() + body_end, [](uint8_t value) { return value != 0; })) {
                unsafe("ID3v2 padding contains non-zero bytes");
            }
            break;
        }
        const size_t frame_header_size = major == 2 ? 6 : 10;
        const size_t identifier_size = major == 2 ? 3 : 4;
        if (cursor + frame_header_size > body_end) unsafe("Truncated ID3v2 frame header");
        for (size_t index = 0; index < identifier_size; ++index) {
            const uint8_t value = tag[cursor + index];
            if (!((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9'))) {
                unsafe("Invalid ID3v2 frame identifier");
            }
        }
        size_t frame_size = 0;
        uint8_t status_flags = 0;
        uint8_t format_flags = 0;
        if (major == 2) {
            frame_size = (static_cast<size_t>(tag[cursor + 3]) << 16) |
                (static_cast<size_t>(tag[cursor + 4]) << 8) | tag[cursor + 5];
        } else {
            frame_size = major == 3
                ? decode_be32(tag.data() + cursor + 4)
                : decode_synchsafe(tag.data() + cursor + 4);
            status_flags = tag[cursor + 8];
            format_flags = tag[cursor + 9];
            if (major == 3 && (((status_flags & 0x1f) != 0) || ((format_flags & 0x1f) != 0))) {
                unsafe("Invalid ID3v2.3 frame flags");
            }
            if (major == 4 && (((status_flags & 0x8f) != 0) || ((format_flags & 0xb0) != 0))) {
                unsafe("Invalid ID3v2.4 frame flags");
            }
            const uint8_t discard_on_tag_alter = major == 3 ? 0x80 : 0x40;
            if ((status_flags & discard_on_tag_alter) != 0) {
                unsafe("ID3v2 frame marked for discard on tag alteration is not safely preservable");
            }
        }
        const size_t payload_start = cursor + frame_header_size;
        const size_t payload_end = payload_start + frame_size;
        if (payload_end > body_end || payload_end < payload_start) unsafe("ID3v2 frame exceeds tag boundary");

        const bool user_text = major == 2
            ? std::memcmp(tag.data() + cursor, "TXX", 3) == 0
            : std::memcmp(tag.data() + cursor, "TXXX", 4) == 0;
        if (user_text) {
            if (format_flags != 0) unsafe("Transformed CUESHEET/TXXX candidates are not safely patchable");
            if (frame_size == 0) unsafe("Empty TXXX frame");
            const unsigned encoding = tag[payload_start];
            if (encoding > 3) unsafe("Unsupported ID3v2 text encoding");
            if (major < 4 && encoding > 1) unsafe("Unsupported text encoding for this ID3v2 version");
            const size_t width = encoding == 1 || encoding == 2 ? 2 : 1;
            const size_t separator = find_separator(tag, payload_start + 1, payload_end, width);
            if (separator == SIZE_MAX) unsafe("TXXX description is not terminated");
            bytes description(tag.begin() + payload_start + 1, tag.begin() + separator);
            if (stricmp_utf8(decode_text(encoding, description).c_str(), "CUESHEET") == 0) {
                if (status_flags != 0) unsafe("Status-flagged CUESHEET frame is not safely patchable");
                ++candidate_count;
                const size_t value_start = separator + width;
                size_t value_end = payload_end;
                bytes terminator;
                if (value_end >= value_start + width &&
                    std::all_of(tag.begin() + value_end - width, tag.begin() + value_end,
                                [](uint8_t value) { return value == 0; })) {
                    value_end -= width;
                    terminator.assign(width, 0);
                }
                if (find_separator(tag, value_start, value_end, width) != SIZE_MAX) {
                    unsafe("CUESHEET TXXX contains multiple values");
                }
                target.frame_start = cursor;
                target.payload_start = payload_start;
                target.payload_end = payload_end;
                target.encoding = encoding;
                target.description = std::move(description);
                target.old_value.assign(tag.begin() + value_start, tag.begin() + value_end);
                target.value_terminator = std::move(terminator);
            }
        }
        cursor = payload_end;
    }

    if (candidate_count != 1) unsafe("Physical ID3v2 tag must contain exactly one CUESHEET TXXX frame");
    const size_t old_padding = body_end - padding_start;
    if (footer_size && old_padding != 0) unsafe("ID3v2 footer tags cannot contain padding");
    if (v23_padding_field != SIZE_MAX && decode_be32(tag.data() + v23_padding_field) != old_padding) {
        unsafe("ID3v2.3 extended-header padding size does not match the tag");
    }
    if (decode_text(target.encoding, target.old_value) != expected_before) {
        unsafe("Raw ID3v2 CUESHEET does not match the prepared foobar2000 snapshot");
    }

    const size_t width = target.encoding == 1 || target.encoding == 2 ? 2 : 1;
    const bytes encoded = encode_text(
        target.encoding, target.old_value, target.description, replacement);
    bytes payload;
    payload.reserve(1 + target.description.size() + width + encoded.size() + target.value_terminator.size());
    payload.push_back(static_cast<uint8_t>(target.encoding));
    payload.insert(payload.end(), target.description.begin(), target.description.end());
    payload.insert(payload.end(), width, 0);
    payload.insert(payload.end(), encoded.begin(), encoded.end());
    payload.insert(payload.end(), target.value_terminator.begin(), target.value_terminator.end());

    bytes used;
    used.reserve(body_size + payload.size());
    used.insert(used.end(), tag.begin() + 10, tag.begin() + target.payload_start);
    used.insert(used.end(), payload.begin(), payload.end());
    used.insert(used.end(), tag.begin() + target.payload_end, tag.begin() + padding_start);
    const size_t new_body_size = std::max(body_size, used.size());
    if (new_body_size >= (1u << 28)) unsafe("Patched ID3v2 tag exceeds the synchsafe size limit");
    const size_t new_padding = new_body_size - used.size();
    if (v23_padding_field != SIZE_MAX) {
        const size_t relative = v23_padding_field - 10;
        encode_be32(pfc::downcast_guarded<uint32_t>(new_padding), used.data() + relative);
    }
    used.insert(used.end(), new_padding, 0);
    const size_t frame_size_offset = target.frame_start - 10 + (major == 2 ? 3 : 4);
    if (major == 2) encode_be24(pfc::downcast_guarded<uint32_t>(payload.size()), used.data() + frame_size_offset);
    else if (major == 3) encode_be32(pfc::downcast_guarded<uint32_t>(payload.size()), used.data() + frame_size_offset);
    else encode_synchsafe(pfc::downcast_guarded<uint32_t>(payload.size()), used.data() + frame_size_offset);

    bytes new_header(tag.begin(), tag.begin() + 10);
    encode_synchsafe(pfc::downcast_guarded<uint32_t>(new_body_size), new_header.data() + 6);
    bytes footer;
    if (footer_size) {
        footer.assign(tag.begin() + body_end, tag.begin() + old_extent);
        encode_synchsafe(pfc::downcast_guarded<uint32_t>(new_body_size), footer.data() + 6);
    }

    file::ptr output;
    filesystem::g_open_write_new(output, output_path, abort);
    output->write_object(new_header.data(), new_header.size(), abort);
    output->write_object(used.data(), used.size(), abort);
    if (!footer.empty()) output->write_object(footer.data(), footer.size(), abort);
    input->seek(old_extent, abort);
    copy_remainder(input, output, abort);
    output.release();
    input.release();

    id3_cuesheet_patch_result result;
    result.major_version = major;
    result.original_tag_extent = old_extent;
    result.patched_tag_extent = 10 + new_body_size + footer_size;
    result.old_padding_bytes = body_end - padding_start;
    result.new_padding_bytes = new_padding;
    return result;
}

} // namespace metadata_isolation
