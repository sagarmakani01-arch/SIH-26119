#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace solver {
namespace json {

class Value {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Value() = default;
  static Value makeNull();
  static Value makeBool(bool b);
  static Value makeNumber(double n);
  static Value makeString(std::string s);
  static Value makeArray();
  static Value makeObject();

  Type type() const { return type_; }
  bool isNull() const { return type_ == Type::Null; }
  bool isBool() const { return type_ == Type::Bool; }
  bool isNumber() const { return type_ == Type::Number; }
  bool isString() const { return type_ == Type::String; }
  bool isArray() const { return type_ == Type::Array; }
  bool isObject() const { return type_ == Type::Object; }

  bool asBool() const { return bool_; }
  double asNumber() const { return number_; }
  const std::string& asString() const { return string_; }

  void push_back(Value v);
  std::size_t size() const;
  const Value& at(std::size_t i) const;

  void set(const std::string& key, Value v);
  const Value* find(const std::string& key) const;

  bool getBool(const std::string& key, bool fallback) const;
  double getNumber(const std::string& key, double fallback) const;
  std::string getString(const std::string& key, const std::string& fallback) const;
  bool has(const std::string& key) const { return find(key) != nullptr; }

  std::string dump() const;

  static Value parse(const std::string& text, std::string* error);

 private:
  void dumpTo(std::string& out) const;

  Type type_ = Type::Null;
  bool bool_ = false;
  double number_ = 0.0;
  std::string string_;
  std::vector<Value> array_;
  std::vector<std::pair<std::string, Value>> object_;
};

std::string escape(const std::string& s);

}  // namespace json
}  // namespace solver
