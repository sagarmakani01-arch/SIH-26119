#pragma once

#include <algorithm>
#include <utility>
#include <vector>

namespace solver {

class LinearExpression {
 public:
  LinearExpression() = default;

  LinearExpression& add(int var, double coefficient) {
    terms_.emplace_back(var, coefficient);
    return *this;
  }

  LinearExpression& constant(double value) {
    constant_ = value;
    return *this;
  }

  int size() const { return static_cast<int>(terms_.size()); }
  double constantValue() const { return constant_; }
  const std::vector<std::pair<int, double>>& terms() const { return terms_; }

  LinearExpression& merge() {
    std::sort(terms_.begin(), terms_.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::pair<int, double>> merged;
    merged.reserve(terms_.size());
    for (const auto& t : terms_) {
      if (!merged.empty() && merged.back().first == t.first) {
        merged.back().second += t.second;
      } else {
        merged.push_back(t);
      }
    }
    merged.erase(std::remove_if(merged.begin(), merged.end(),
                                [](const auto& t) { return t.second == 0.0; }),
                 merged.end());
    terms_.swap(merged);
    return *this;
  }

  LinearExpression operator+(const LinearExpression& other) const {
    LinearExpression r;
    r.terms_ = terms_;
    r.terms_.insert(r.terms_.end(), other.terms_.begin(), other.terms_.end());
    r.constant_ = constant_ + other.constant_;
    r.merge();
    return r;
  }

  LinearExpression operator*(double scale) const {
    LinearExpression r;
    r.terms_.reserve(terms_.size());
    for (const auto& t : terms_) r.terms_.emplace_back(t.first, t.second * scale);
    r.constant_ = constant_ * scale;
    return r;
  }

 private:
  std::vector<std::pair<int, double>> terms_;
  double constant_ = 0.0;
};

}  // namespace solver
