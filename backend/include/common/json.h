#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

class Value {
public:
    enum Type { Null, Bool, Int, Double, String, ArrayType, ObjectType };

    Value() : type_(Null) {}
    Value(std::nullptr_t) : type_(Null) {}
    Value(bool v) : type_(Bool), bool_val_(v) {}
    Value(int v) : type_(Int), int_val_(v) {}
    Value(long v) : type_(Int), int_val_(v) {}
    Value(long long v) : type_(Int), int_val_(static_cast<int64_t>(v)) {}
    Value(unsigned v) : type_(Int), int_val_(static_cast<int64_t>(v)) {}
    Value(unsigned long v) : type_(Int), int_val_(static_cast<int64_t>(v)) {}
    Value(unsigned long long v) : type_(Int), int_val_(static_cast<int64_t>(v)) {}
    Value(int64_t v) : type_(Int), int_val_(v) {}
    Value(double v) : type_(Double), double_val_(v) {}
    Value(float v) : type_(Double), double_val_(v) {}
    Value(const char* v) : type_(String), string_val_(v) {}
    Value(const std::string& v) : type_(String), string_val_(v) {}
    Value(std::string&& v) : type_(String), string_val_(std::move(v)) {}
    Value(const struct json::Array& v) : type_(ArrayType), array_val_(v) {}
    Value(struct json::Array&& v) : type_(ArrayType), array_val_(std::move(v)) {}
    Value(const struct json::Object& v) : type_(ObjectType), object_val_(v) {}
    Value(struct json::Object&& v) : type_(ObjectType), object_val_(std::move(v)) {}

    Type type() const { return type_; }
    bool isNull() const { return type_ == Null; }
    bool isBool() const { return type_ == Bool; }
    bool isInt() const { return type_ == Int; }
    bool isDouble() const { return type_ == Double; }
    bool isNumeric() const { return type_ == Int || type_ == Double; }
    bool isString() const { return type_ == String; }
    bool isArray() const { return type_ == ArrayType; }
    bool isObject() const { return type_ == ObjectType; }

    bool asBool() const {
        if (type_ == Bool) return bool_val_;
        if (type_ == Int) return int_val_ != 0;
        return false;
    }
    int64_t asInt64() const {
        if (type_ == Int) return int_val_;
        if (type_ == Double) return static_cast<int64_t>(double_val_);
        if (type_ == Bool) return bool_val_ ? 1 : 0;
        return 0;
    }
    int asInt() const { return static_cast<int>(asInt64()); }
    unsigned int asUInt() const { return static_cast<unsigned int>(asInt64()); }
    double asDouble() const {
        if (type_ == Double) return double_val_;
        if (type_ == Int) return static_cast<double>(int_val_);
        return 0.0;
    }
    const std::string& asString() const {
        static const std::string empty;
        return type_ == String ? string_val_ : empty;
    }
    const struct json::Array& asArray() const {
        static const struct json::Array empty;
        return type_ == ArrayType ? array_val_ : empty;
    }
    struct json::Array& asArray() { return array_val_; }
    const struct json::Object& asObject() const {
        static const struct json::Object empty;
        return type_ == ObjectType ? object_val_ : empty;
    }
    struct json::Object& asObject() { return object_val_; }

    bool has(const std::string& key) const {
        return type_ == ObjectType && object_val_.find(key) != object_val_.end();
    }

    const Value& operator[](const std::string& key) const {
        static const Value null_val;
        if (type_ != ObjectType) return null_val;
        auto it = object_val_.find(key);
        return it != object_val_.end() ? it->second : null_val;
    }

    Value& operator[](const std::string& key) {
        if (type_ != ObjectType) { type_ = ObjectType; object_val_.clear(); }
        return object_val_[key];
    }

    const Value& operator[](size_t idx) const {
        static const Value null_val;
        if (type_ != ArrayType || idx >= array_val_.size()) return null_val;
        return array_val_[idx];
    }

    Value& operator[](size_t idx) { return array_val_[idx]; }

    size_t size() const {
        if (type_ == ArrayType) return array_val_.size();
        if (type_ == ObjectType) return object_val_.size();
        return 0;
    }

    void append(const Value& v) {
        if (type_ != ArrayType) { type_ = ArrayType; array_val_.clear(); }
        array_val_.push_back(v);
    }

    std::string dump() const;

private:
    Type type_;
    bool bool_val_ = false;
    int64_t int_val_ = 0;
    double double_val_ = 0.0;
    std::string string_val_;
    struct json::Array array_val_;
    struct json::Object object_val_;

    static void escape_string(std::ostream& os, const std::string& s) {
        os << '"';
        for (char c : s) {
            switch (c) {
                case '"':  os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\n': os << "\\n"; break;
                case '\r': os << "\\r"; break;
                case '\t': os << "\\t"; break;
                case '\b': os << "\\b"; break;
                case '\f': os << "\\f"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        os << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                    } else {
                        os << c;
                    }
            }
        }
        os << '"';
    }

    void dump_impl(std::ostream& os) const {
        switch (type_) {
            case Null: os << "null"; break;
            case Bool: os << (bool_val_ ? "true" : "false"); break;
            case Int: os << int_val_; break;
            case Double:
                os << std::setprecision(15) << double_val_;
                break;
            case String: escape_string(os, string_val_); break;
            case ArrayType:
                os << "[";
                for (size_t i = 0; i < array_val_.size(); ++i) {
                    if (i) os << ",";
                    array_val_[i].dump_impl(os);
                }
                os << "]";
                break;
            case ObjectType:
                os << "{";
                bool first = true;
                for (const auto& [k, v] : object_val_) {
                    if (!first) os << ",";
                    first = false;
                    escape_string(os, k);
                    os << ":";
                    v.dump_impl(os);
                }
                os << "}";
                break;
        }
    }
};

inline std::string Value::dump() const {
    std::ostringstream oss;
    dump_impl(oss);
    return oss.str();
}

inline Value parse(const std::string& s);

namespace detail {

class Parser {
public:
    Parser(const std::string& input) : input_(input), pos_(0) {}

    Value parse() {
        skip_ws();
        Value v = parse_value();
        return v;
    }

private:
    const std::string& input_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_;
    }

    char peek() { return pos_ < input_.size() ? input_[pos_] : '\0'; }

    char get() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }

    Value parse_value() {
        skip_ws();
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        return Value();
    }

    Value parse_object() {
        Object obj;
        get();
        skip_ws();
        if (peek() == '}') { get(); return Value(std::move(obj)); }
        while (true) {
            skip_ws();
            Value key_v = parse_string();
            std::string key = key_v.asString();
            skip_ws();
            if (get() != ':') break;
            skip_ws();
            Value val = parse_value();
            obj[key] = std::move(val);
            skip_ws();
            char c = get();
            if (c == '}') break;
            if (c != ',') break;
        }
        return Value(std::move(obj));
    }

    Value parse_array() {
        Array arr;
        get();
        skip_ws();
        if (peek() == ']') { get(); return Value(std::move(arr)); }
        while (true) {
            skip_ws();
            arr.push_back(parse_value());
            skip_ws();
            char c = get();
            if (c == ']') break;
            if (c != ',') break;
        }
        return Value(std::move(arr));
    }

    Value parse_string() {
        std::string s;
        get();
        while (pos_ < input_.size()) {
            char c = get();
            if (c == '"') return Value(std::move(s));
            if (c == '\\') {
                char esc = get();
                switch (esc) {
                    case '"':  s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/':  s += '/'; break;
                    case 'n':  s += '\n'; break;
                    case 'r':  s += '\r'; break;
                    case 't':  s += '\t'; break;
                    case 'b':  s += '\b'; break;
                    case 'f':  s += '\f'; break;
                    case 'u': {
                        if (pos_ + 4 <= input_.size()) {
                            std::string hex = input_.substr(pos_, 4);
                            pos_ += 4;
                            try {
                                int cp = std::stoi(hex, nullptr, 16);
                                if (cp < 0x80) s += static_cast<char>(cp);
                                else if (cp < 0x800) {
                                    s += static_cast<char>(0xC0 | (cp >> 6));
                                    s += static_cast<char>(0x80 | (cp & 0x3F));
                                } else {
                                    s += static_cast<char>(0xE0 | (cp >> 12));
                                    s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                    s += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                            } catch (...) {}
                        }
                        break;
                    }
                    default: s += esc;
                }
            } else {
                s += c;
            }
        }
        return Value(std::move(s));
    }

    Value parse_bool() {
        if (input_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value(true); }
        if (input_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value(false); }
        return Value();
    }

    Value parse_null() {
        if (input_.compare(pos_, 4, "null") == 0) pos_ += 4;
        return Value();
    }

    Value parse_number() {
        size_t start = pos_;
        if (peek() == '-') get();
        while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        bool is_double = false;
        if (peek() == '.') {
            is_double = true;
            get();
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }
        if (peek() == 'e' || peek() == 'E') {
            is_double = true;
            get();
            if (peek() == '+' || peek() == '-') get();
            while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        }
        std::string num = input_.substr(start, pos_ - start);
        try {
            if (is_double) {
                return Value(std::stod(num));
            } else {
                return Value(static_cast<int64_t>(std::stoll(num)));
            }
        } catch (...) {
            return Value();
        }
    }
};

}

inline Value parse(const std::string& s) {
    detail::Parser parser(s);
    return parser.parse();
}

}
