#include "solver/io/json.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>

namespace solver {
namespace json {

Value Value::makeNull() { return Value(); }

Value Value::makeBool(bool b) {
  Value v;
  v.type_ = Type::Bool;
  v.bool_ = b;
  return v;
}

Value Value::makeNumber(double n) {
  Value v;
  v.type_ = Type::Number;
  v.number_ = n;
  return v;
}

Value Value::makeString(std::string s) {
  Value v;
  v.type_ = Type::String;
  v.string_ = std::move(s);
  return v;
}

Value Value::makeArray() {
  Value v;
  v.type_ = Type::Array;
  return v;
}

Value Value::makeObject() {
  Value v;
  v.type_ = Type::Object;
  return v;
}

void Value::push_back(Value v) {
  if (type_ != Type::Array) {
    *this = makeArray();
  }
  array_.push_back(std::move(v));
}

std::size_t Value::size() const {
  if (type_ == Type::Array) return array_.size();
  if (type_ == Type::Object) return object_.size();
  return 0;
}

const Value& Value::at(std::size_t i) const {
  static const Value null_value;
  if (type_ != Type::Array || i >= array_.size()) return null_value;
  return array_[i];
}

void Value::set(const std::string& key, Value v) {
  if (type_ != Type::Object) {
    *this = makeObject();
  }
  for (auto& kv : object_) {
    if (kv.first == key) {
      kv.second = std::move(v);
      return;
    }
  }
  object_.emplace_back(key, std::move(v));
}

const Value* Value::find(const std::string& key) const {
  if (type_ != Type::Object) return nullptr;
  for (const auto& kv : object_) {
    if (kv.first == key) return &kv.second;
  }
  return nullptr;
}

bool Value::getBool(const std::string& key, bool fallback) const {
  const Value* v = find(key);
  if (!v) return fallback;
  if (v->type_ == Type::Bool) return v->bool_;
  if (v->type_ == Type::Number) return v->number_ != 0.0;
  return fallback;
}

double Value::getNumber(const std::string& key, double fallback) const {
  const Value* v = find(key);
  if (!v || v->type_ != Type::Number) return fallback;
  return v->number_;
}

std::string Value::getString(const std::string& key, const std::string& fallback) const {
  const Value* v = find(key);
  if (!v || v->type_ != Type::String) return fallback;
  return v->string_;
}

std::string escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

namespace {

void dumpNumber(double n, std::string& out) {
  if (!std::isfinite(n)) {
    out += "null";
    return;
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", n);
  double back = std::strtod(buf, nullptr);
  if (back == n) {
    std::snprintf(buf, sizeof(buf), "%.15g", n);
    back = std::strtod(buf, nullptr);
    if (back == n) {
      std::snprintf(buf, sizeof(buf), "%.9g", n);
      back = std::strtod(buf, nullptr);
      if (back == n) {
        out += buf;
        return;
      }
    }
  }
  std::snprintf(buf, sizeof(buf), "%.17g", n);
  out += buf;
}

class Parser {
 public:
  Parser(const std::string& text) : s_(text) {}

  bool parse(Value& out) {
    skipWs();
    if (!parseValue(out)) return false;
    skipWs();
    if (pos_ != s_.size()) {
      error_ = "trailing characters at position " + std::to_string(pos_);
      return false;
    }
    return true;
  }

  const std::string& error() const { return error_; }

 private:
  void skipWs() {
    while (pos_ < s_.size() &&
           (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) {
      ++pos_;
    }
  }

  bool fail(const std::string& msg) {
    if (error_.empty()) error_ = msg + " at position " + std::to_string(pos_);
    return false;
  }

  bool literal(const char* lit) {
    const std::size_t n = std::char_traits<char>::length(lit);
    if (s_.compare(pos_, n, lit) != 0) return fail("invalid literal");
    pos_ += n;
    return true;
  }

  bool parseValue(Value& out) {
    if (pos_ >= s_.size()) return fail("unexpected end of input");
    const char c = s_[pos_];
    if (c == '{') return parseObject(out);
    if (c == '[') return parseArray(out);
    if (c == '"') {
      std::string str;
      if (!parseString(str)) return false;
      out = Value::makeString(std::move(str));
      return true;
    }
    if (c == 't') {
      if (!literal("true")) return false;
      out = Value::makeBool(true);
      return true;
    }
    if (c == 'f') {
      if (!literal("false")) return false;
      out = Value::makeBool(false);
      return true;
    }
    if (c == 'n') {
      if (!literal("null")) return false;
      out = Value::makeNull();
      return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out);
    return fail("unexpected character");
  }

  bool parseObject(Value& out) {
    out = Value::makeObject();
    ++pos_;
    skipWs();
    if (pos_ < s_.size() && s_[pos_] == '}') {
      ++pos_;
      return true;
    }
    while (true) {
      skipWs();
      if (pos_ >= s_.size() || s_[pos_] != '"') return fail("expected object key");
      std::string key;
      if (!parseString(key)) return false;
      skipWs();
      if (pos_ >= s_.size() || s_[pos_] != ':') return fail("expected ':'");
      ++pos_;
      skipWs();
      Value v;
      if (!parseValue(v)) return false;
      out.set(key, std::move(v));
      skipWs();
      if (pos_ < s_.size() && s_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (pos_ < s_.size() && s_[pos_] == '}') {
        ++pos_;
        return true;
      }
      return fail("expected ',' or '}'");
    }
  }

  bool parseArray(Value& out) {
    out = Value::makeArray();
    ++pos_;
    skipWs();
    if (pos_ < s_.size() && s_[pos_] == ']') {
      ++pos_;
      return true;
    }
    while (true) {
      skipWs();
      Value v;
      if (!parseValue(v)) return false;
      out.push_back(std::move(v));
      skipWs();
      if (pos_ < s_.size() && s_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (pos_ < s_.size() && s_[pos_] == ']') {
        ++pos_;
        return true;
      }
      return fail("expected ',' or ']'");
    }
  }

  bool parseString(std::string& out) {
    ++pos_;
    out.clear();
    while (pos_ < s_.size()) {
      const unsigned char c = static_cast<unsigned char>(s_[pos_]);
      if (c == '"') {
        ++pos_;
        return true;
      }
      if (c == '\\') {
        ++pos_;
        if (pos_ >= s_.size()) return fail("bad escape");
        const char e = s_[pos_];
        switch (e) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'n': out += '\n'; break;
          case 'r': out += '\r'; break;
          case 't': out += '\t'; break;
          case 'u': {
            if (pos_ + 4 >= s_.size()) return fail("bad \\u escape");
            unsigned code = 0;
            for (int i = 1; i <= 4; ++i) {
              const char h = s_[pos_ + static_cast<std::size_t>(i)];
              code <<= 4;
              if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
              else return fail("bad hex digit in \\u escape");
            }
            pos_ += 4;
            if (code < 0x80) {
              out += static_cast<char>(code);
            } else if (code < 0x800) {
              out += static_cast<char>(0xC0 | (code >> 6));
              out += static_cast<char>(0x80 | (code & 0x3F));
            } else {
              out += static_cast<char>(0xE0 | (code >> 12));
              out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
              out += static_cast<char>(0x80 | (code & 0x3F));
            }
            break;
          }
          default: return fail("unknown escape");
        }
        ++pos_;
        continue;
      }
      if (c < 0x20) return fail("unescaped control character in string");
      out += s_[pos_];
      ++pos_;
    }
    return fail("unterminated string");
  }

  bool parseNumber(Value& out) {
    const std::size_t start = pos_;
    if (pos_ < s_.size() && s_[pos_] == '-') ++pos_;
    while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    if (pos_ < s_.size() && s_[pos_] == '.') {
      ++pos_;
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    }
    if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    }
    const std::string token = s_.substr(start, pos_ - start);
    char* end = nullptr;
    const double v = std::strtod(token.c_str(), &end);
    if (end == token.c_str() || *end != '\0') return fail("invalid number");
    out = Value::makeNumber(v);
    return true;
  }

  const std::string& s_;
  std::size_t pos_ = 0;
  std::string error_;
};

}  // namespace

void Value::dumpTo(std::string& out) const {
  switch (type_) {
    case Type::Null:
      out += "null";
      break;
    case Type::Bool:
      out += bool_ ? "true" : "false";
      break;
    case Type::Number:
      dumpNumber(number_, out);
      break;
    case Type::String:
      out += '"';
      out += escape(string_);
      out += '"';
      break;
    case Type::Array: {
      out += '[';
      for (std::size_t i = 0; i < array_.size(); ++i) {
        if (i) out += ',';
        array_[i].dumpTo(out);
      }
      out += ']';
      break;
    }
    case Type::Object: {
      out += '{';
      for (std::size_t i = 0; i < object_.size(); ++i) {
        if (i) out += ',';
        out += '"';
        out += escape(object_[i].first);
        out += "\":";
        object_[i].second.dumpTo(out);
      }
      out += '}';
      break;
    }
  }
}

std::string Value::dump() const {
  std::string out;
  dumpTo(out);
  return out;
}

Value Value::parse(const std::string& text, std::string* error) {
  Parser p(text);
  Value out;
  if (!p.parse(out)) {
    if (error) *error = p.error();
    return Value::makeNull();
  }
  if (error) error->clear();
  return out;
}

}  // namespace json
}  // namespace solver
