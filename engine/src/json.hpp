#ifndef POECRAFT_SRC_JSON_HPP
#define POECRAFT_SRC_JSON_HPP

/*
 * Minimal JSON DOM parser for the engine data loader.
 *
 * The compiled runtime artifact is machine-generated (see
 * tools/ingest/poecraft_ingest/compiled_data.py), so this parser only needs to
 * handle well-formed JSON, not arbitrary hostile input. It builds a full DOM;
 * the loader extracts typed arrays and then frees the DOM. The eventual binary
 * compiled format (Phase 14) replaces this JSON-first path for production hot
 * loading; for the foundation milestone a clear, correct DOM is the priority.
 */

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace poecraft::json {

struct Value;
using Array = std::vector<Value>;
using Member = std::pair<std::string, Value>;
using Object = std::vector<Member>;

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    Array array;
    Object object;

    bool is_null() const { return type == Type::Null; }

    const Value* find(const std::string& key) const {
        if (type != Type::Object) {
            return nullptr;
        }
        for (const auto& member : object) {
            if (member.first == key) {
                return &member.second;
            }
        }
        return nullptr;
    }

    const Value& at(const std::string& key) const {
        const Value* found = find(key);
        if (found == nullptr) {
            throw std::runtime_error("missing JSON key: " + key);
        }
        return *found;
    }

    bool as_bool() const {
        if (type != Type::Bool) {
            throw std::runtime_error("expected JSON bool");
        }
        return boolean;
    }

    double as_number() const {
        if (type != Type::Number) {
            throw std::runtime_error("expected JSON number");
        }
        return number;
    }

    std::int64_t as_int() const {
        return static_cast<std::int64_t>(as_number());
    }

    const std::string& as_string() const {
        if (type != Type::String) {
            throw std::runtime_error("expected JSON string");
        }
        return string;
    }

    const Array& as_array() const {
        if (type != Type::Array) {
            throw std::runtime_error("expected JSON array");
        }
        return array;
    }
};

class Parser {
public:
    Parser(const char* data, std::size_t size) : data_(data), size_(size) {}

    Value parse() {
        skip_ws();
        Value root = parse_value();
        skip_ws();
        if (pos_ != size_) {
            fail("trailing content after JSON document");
        }
        return root;
    }

private:
    const char* data_;
    std::size_t size_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const char* message) {
        throw std::runtime_error(
            std::string("JSON parse error at offset ") +
            std::to_string(pos_) + ": " + message);
    }

    char peek() {
        if (pos_ >= size_) {
            fail("unexpected end of input");
        }
        return data_[pos_];
    }

    char get() {
        char c = peek();
        ++pos_;
        return c;
    }

    void expect(char c) {
        if (get() != c) {
            fail("unexpected character");
        }
    }

    void skip_ws() {
        while (pos_ < size_) {
            char c = data_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    Value parse_value() {
        skip_ws();
        char c = peek();
        switch (c) {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"': {
            Value v;
            v.type = Type::String;
            v.string = parse_string();
            return v;
        }
        case 't':
        case 'f':
            return parse_bool();
        case 'n':
            return parse_null();
        default:
            return parse_number();
        }
    }

    Value parse_object() {
        Value v;
        v.type = Type::Object;
        expect('{');
        skip_ws();
        if (peek() == '}') {
            ++pos_;
            return v;
        }
        while (true) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            Value child = parse_value();
            v.object.emplace_back(std::move(key), std::move(child));
            skip_ws();
            char c = get();
            if (c == ',') {
                continue;
            }
            if (c == '}') {
                break;
            }
            fail("expected ',' or '}' in object");
        }
        return v;
    }

    Value parse_array() {
        Value v;
        v.type = Type::Array;
        expect('[');
        skip_ws();
        if (peek() == ']') {
            ++pos_;
            return v;
        }
        while (true) {
            Value child = parse_value();
            v.array.push_back(std::move(child));
            skip_ws();
            char c = get();
            if (c == ',') {
                continue;
            }
            if (c == ']') {
                break;
            }
            fail("expected ',' or ']' in array");
        }
        return v;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (pos_ >= size_) {
                fail("unterminated string");
            }
            char c = data_[pos_++];
            if (c == '"') {
                break;
            }
            if (c == '\\') {
                if (pos_ >= size_) {
                    fail("unterminated escape");
                }
                char e = data_[pos_++];
                switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': out += parse_unicode_escape(); break;
                default: fail("invalid escape sequence");
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

    std::string parse_unicode_escape() {
        unsigned code = read_hex4();
        if (code >= 0xD800 && code <= 0xDBFF) {
            /* high surrogate; expect a following \uXXXX low surrogate */
            if (pos_ + 1 >= size_ || data_[pos_] != '\\' ||
                data_[pos_ + 1] != 'u') {
                fail("expected low surrogate");
            }
            pos_ += 2;
            unsigned low = read_hex4();
            if (low < 0xDC00 || low > 0xDFFF) {
                fail("invalid low surrogate");
            }
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        }
        return encode_utf8(code);
    }

    unsigned read_hex4() {
        if (pos_ + 4 > size_) {
            fail("truncated unicode escape");
        }
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            char c = data_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                fail("invalid hex digit");
            }
        }
        return value;
    }

    static std::string encode_utf8(unsigned code) {
        std::string out;
        if (code <= 0x7F) {
            out.push_back(static_cast<char>(code));
        } else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        return out;
    }

    Value parse_bool() {
        Value v;
        v.type = Type::Bool;
        if (size_ - pos_ >= 4 && std::string(data_ + pos_, 4) == "true") {
            v.boolean = true;
            pos_ += 4;
        } else if (size_ - pos_ >= 5 &&
                   std::string(data_ + pos_, 5) == "false") {
            v.boolean = false;
            pos_ += 5;
        } else {
            fail("invalid literal");
        }
        return v;
    }

    Value parse_null() {
        if (size_ - pos_ >= 4 && std::string(data_ + pos_, 4) == "null") {
            pos_ += 4;
            return Value{};
        }
        fail("invalid literal");
    }

    Value parse_number() {
        std::size_t start = pos_;
        if (pos_ < size_ && (data_[pos_] == '-' || data_[pos_] == '+')) {
            ++pos_;
        }
        while (pos_ < size_) {
            char c = data_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '+' || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }
        if (pos_ == start) {
            fail("invalid number");
        }
        Value v;
        v.type = Type::Number;
        v.number = std::stod(std::string(data_ + start, pos_ - start));
        return v;
    }
};

/* Read an array of integers into out. */
inline void read_int_array(const Value& value, std::vector<std::int64_t>& out) {
    const Array& arr = value.as_array();
    out.clear();
    out.reserve(arr.size());
    for (const auto& element : arr) {
        out.push_back(element.as_int());
    }
}

inline void read_int_array(const Value& value, std::vector<std::int32_t>& out) {
    const Array& arr = value.as_array();
    out.clear();
    out.reserve(arr.size());
    for (const auto& element : arr) {
        out.push_back(static_cast<std::int32_t>(element.as_int()));
    }
}

inline void read_int_array(const Value& value, std::vector<std::uint32_t>& out) {
    const Array& arr = value.as_array();
    out.clear();
    out.reserve(arr.size());
    for (const auto& element : arr) {
        out.push_back(static_cast<std::uint32_t>(element.as_int()));
    }
}

} // namespace poecraft::json

#endif
