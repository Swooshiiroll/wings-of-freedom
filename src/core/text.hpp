#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>

namespace wof {

// Small helpers for the `key = value` text formats (the tuning file). Pure string handling;
// reading and writing files is the front end's job.

[[nodiscard]] inline std::string_view trim(std::string_view s) noexcept {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

// A whole decimal integer (optionally signed, within +-1e9), nothing else.
[[nodiscard]] inline bool parse_int(std::string_view s, int& out) {
    const std::string str(trim(s));
    if (str.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(str.c_str(), &end, 10);
    if (end == str.c_str() || *end != '\0' || errno != 0 || v < -1000000000L || v > 1000000000L) {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

// A finite decimal number within +-1e9 ("110", "2.5", "-0.4"), nothing else. Locale-independent,
// and it reads back exactly what format_float writes.
[[nodiscard]] inline bool parse_float(std::string_view s, float& out) {
    const std::string_view str = trim(s);
    if (str.empty()) {
        return false;
    }
    float v = 0.0f;
    const auto [end, ec] = std::from_chars(str.data(), str.data() + str.size(), v);
    if (ec != std::errc{} || end != str.data() + str.size() || !std::isfinite(v) || v < -1e9f ||
        v > 1e9f) {
        return false;
    }
    out = v;
    return true;
}

// The shortest text that parse_float reads back as exactly `v`.
[[nodiscard]] inline std::string format_float(float v) {
    char buf[32];
    const auto [end, ec] = std::to_chars(buf, buf + sizeof buf, v);
    return ec == std::errc{} ? std::string(buf, end) : std::string("0");
}

// An unsigned 64-bit number, decimal or hex with 0x.
[[nodiscard]] inline bool parse_u64(std::string_view s, std::uint64_t& out) {
    const std::string str(trim(s));
    if (str.empty() || str[0] == '-' || str[0] == '+') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long long v = std::strtoull(str.c_str(), &end, 0);
    if (end == str.c_str() || *end != '\0' || errno != 0) {
        return false;
    }
    out = static_cast<std::uint64_t>(v);
    return true;
}

// One `key = value` line of a text: the key and value trimmed, `#` comments dropped. Call
// next() until it returns false; blank lines are skipped, and `bad` is set for a line that isn't
// `key = value`.
class KeyValueLines {
  public:
    explicit KeyValueLines(std::string_view text) noexcept : text_(text) {}

    bool next() noexcept {
        while (!text_.empty()) {
            ++line_;
            const auto nl = text_.find('\n');
            const std::string_view raw = text_.substr(0, nl);
            text_ = nl == std::string_view::npos ? std::string_view{} : text_.substr(nl + 1);
            const auto hash = raw.find('#');
            const std::string_view body =
                trim(hash == std::string_view::npos ? raw : raw.substr(0, hash));
            if (body.empty()) {
                continue;
            }
            const auto eq = body.find('=');
            bad_ = eq == std::string_view::npos;
            key_ = bad_ ? body : trim(body.substr(0, eq));
            value_ = bad_ ? std::string_view{} : trim(body.substr(eq + 1));
            return true;
        }
        return false;
    }

    [[nodiscard]] int line() const noexcept { return line_; }
    [[nodiscard]] bool bad() const noexcept { return bad_; }
    [[nodiscard]] std::string_view key() const noexcept { return key_; }
    [[nodiscard]] std::string_view value() const noexcept { return value_; }

  private:
    std::string_view text_;
    std::string_view key_;
    std::string_view value_;
    int line_ = 0;
    bool bad_ = false;
};

// Appends "line N: what" and a newline to `errors`.
inline void report_line(std::string& errors, int line, std::string_view what) {
    errors.append("line ").append(std::to_string(line)).append(": ").append(what).append("\n");
}

} // namespace wof
