// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew Glazar
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef QUICK_LINT_JS_EXPRESSION_H
#define QUICK_LINT_JS_EXPRESSION_H

#include <array>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <deque>
#include <memory>
#include <optional>
#include <quick-lint-js/buffering-visitor.h>
#include <quick-lint-js/lex.h>
#include <quick-lint-js/location.h>
#include <quick-lint-js/narrow-cast.h>
#include <quick-lint-js/unreachable.h>
#include <quick-lint-js/warning.h>
#include <type_traits>
#include <utility>
#include <vector>

#define QLJS_UNEXPECTED_EXPRESSION_KIND()                                 \
  do {                                                                    \
    assert(false && "function not implemented for this expression kind"); \
    QLJS_UNREACHABLE();                                                   \
  } while (false)

QLJS_WARNING_PUSH
QLJS_WARNING_IGNORE_CLANG("-Wnon-virtual-dtor")
QLJS_WARNING_IGNORE_GCC("-Wnon-virtual-dtor")

namespace quick_lint_js {
class expression;

class expression_ptr {
 public:
  explicit expression_ptr(expression *ptr) noexcept : ptr_(ptr) {}

  expression *operator->() const noexcept { return this->ptr_; }
  expression &operator*() const noexcept { return *this->ptr_; }

  bool operator==(expression_ptr other) const noexcept {
    return this->ptr_ == other.ptr_;
  }

  bool operator!=(expression_ptr other) const noexcept {
    return !(*this == other);
  }

 private:
  expression *ptr_;
};

enum class expression_kind {
  _invalid,
  _new,
  _template,
  array,
  arrow_function_with_expression,
  arrow_function_with_statements,
  assignment,
  await,
  binary_operator,
  call,
  compound_assignment,
  conditional,
  dot,
  function,
  import,
  index,
  literal,
  named_function,
  object,
  rw_unary_prefix,
  rw_unary_suffix,
  spread,
  super,
  unary_operator,
  variable,
};

struct object_property_value_pair {
  explicit object_property_value_pair(expression_ptr property,
                                      expression_ptr value) noexcept
      : property(property), value(value) {}

  explicit object_property_value_pair(expression_ptr value) noexcept
      : property(std::nullopt), value(value) {}

  std::optional<expression_ptr> property;
  expression_ptr value;
};

class expression_arena {
 public:
  template <class>
  class array_ptr;

  using buffering_visitor_ptr = buffering_visitor *;

  template <class T>
  static inline constexpr bool is_allocatable =
      std::is_trivially_destructible_v<std::remove_reference_t<T>>;

  template <class Expression, class... Args>
  expression_ptr make_expression(Args &&... args);

  array_ptr<expression_ptr> make_array(
      std::vector<expression_ptr> &&expressions);

  array_ptr<object_property_value_pair> make_array(
      std::vector<object_property_value_pair> &&pairs);

  buffering_visitor_ptr make_buffering_visitor(
      std::unique_ptr<buffering_visitor> &&visitor) {
    // See matching delete in delete_buffering_visitor.
    return visitor.release();
  }

  void delete_buffering_visitor(buffering_visitor_ptr visitor) {
    // See matching allocation in make_buffering_visitor.
    delete visitor;
  }

 private:
  template <class T, class... Args>
  T *allocate(Args &&... args) {
    static_assert(is_allocatable<T>);
    boost::container::pmr::polymorphic_allocator<T> allocator(&this->memory_);
    T *result = allocator.allocate(1);
    result = new (result) T(std::forward<Args>(args)...);
    return result;
  }

  std::deque<std::vector<expression_ptr>> expression_ptr_arrays_;
  std::deque<std::vector<object_property_value_pair>> object_pair_arrays_;
  boost::container::pmr::monotonic_buffer_resource memory_;
};

class expression {
 public:
  class expression_with_prefix_operator_base;

  class _invalid;
  class _new;
  class _template;
  class array;
  class arrow_function_with_expression;
  class arrow_function_with_statements;
  class assignment;
  class await;
  class binary_operator;
  class call;
  class conditional;
  class dot;
  class function;
  class import;
  class index;
  class literal;
  class named_function;
  class object;
  class rw_unary_prefix;
  class rw_unary_suffix;
  class spread;
  class super;
  class unary_operator;
  class variable;

  expression_kind kind() const noexcept { return this->kind_; }

  virtual identifier variable_identifier() const noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

  virtual int child_count() const noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

  expression_ptr child_0() const noexcept { return this->child(0); }
  expression_ptr child_1() const noexcept { return this->child(1); }
  expression_ptr child_2() const noexcept { return this->child(2); }

  virtual expression_ptr child(int) const noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

  // Can be called at most once.
  template <class Visitor>
  void visit_children(Visitor &v, expression_arena &arena) {
    buffering_visitor *child_visits = this->take_child_visits();
    assert(
        child_visits &&
        "visit_children can be called at most once, but it was called twice");
    child_visits->move_into(v);
    arena.delete_buffering_visitor(child_visits);
  }

  virtual int object_entry_count() const noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

  virtual object_property_value_pair object_entry(int) const noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

  virtual source_code_span span() const noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

  virtual function_attributes attributes() const noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

 protected:
  explicit expression(expression_kind kind) noexcept : kind_(kind) {}

  // Returns expression_arena::buffering_visitor_ptr.
  virtual buffering_visitor *take_child_visits() noexcept {
    QLJS_UNEXPECTED_EXPRESSION_KIND();
  }

 private:
  expression_kind kind_;
};

template <class T>
class expression_arena::array_ptr {
 public:
  explicit array_ptr() noexcept : data_(nullptr), size_(0) {}

  explicit array_ptr(const T *data, int size) noexcept
      : data_(data), size_(size) {}

  T operator[](int index) const noexcept {
    assert(index >= 0);
    assert(index < this->size());
    return this->data_[index];
  }

  T front() const noexcept { return (*this)[0]; }

  T back() const noexcept { return (*this)[this->size() - 1]; }

  const T *data() const noexcept { return this->data_; }

  int size() const noexcept { return this->size_; }

  bool empty() const noexcept { return this->size() == 0; }

 private:
  const T *data_;
  int size_;
};

template <class Expression, class... Args>
expression_ptr expression_arena::make_expression(Args &&... args) {
  expression_ptr result(
      this->allocate<Expression>(std::forward<Args>(args)...));
  static_assert(is_allocatable<Expression>);
  return result;
}

inline expression_arena::array_ptr<expression_ptr> expression_arena::make_array(
    std::vector<expression_ptr> &&expressions) {
  this->expression_ptr_arrays_.emplace_back(std::move(expressions));
  std::vector<expression_ptr> &stored_expressions =
      this->expression_ptr_arrays_.back();
  array_ptr<expression_ptr> result(stored_expressions.data(),
                                   narrow_cast<int>(stored_expressions.size()));
  static_assert(is_allocatable<decltype(result[0])>);
  return result;
}

inline expression_arena::array_ptr<object_property_value_pair>
expression_arena::make_array(std::vector<object_property_value_pair> &&pairs) {
  this->object_pair_arrays_.emplace_back(std::move(pairs));
  std::vector<object_property_value_pair> &stored_pairs =
      this->object_pair_arrays_.back();
  array_ptr<object_property_value_pair> result(
      stored_pairs.data(), narrow_cast<int>(stored_pairs.size()));
  static_assert(is_allocatable<decltype(result[0])>);
  return result;
}

class expression::expression_with_prefix_operator_base : public expression {
 public:
  explicit expression_with_prefix_operator_base(
      expression_kind kind, expression_ptr child,
      source_code_span operator_span) noexcept
      : expression(kind),
        unary_operator_begin_(operator_span.begin()),
        child_(child) {}

  int child_count() const noexcept override { return 1; }

  expression_ptr child([[maybe_unused]] int index) const noexcept override {
    assert(index == 0);
    return this->child_;
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->unary_operator_begin_,
                            this->child_->span().end());
  }

 private:
  const char *unary_operator_begin_;
  expression_ptr child_;
};

class expression::_invalid final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::_invalid;

  explicit _invalid() noexcept : expression(kind) {}

  source_code_span span() const noexcept override {
    assert(false && "Not yet implemented");
    QLJS_UNREACHABLE();
  }
};
static_assert(expression_arena::is_allocatable<expression::_invalid>);

class expression::_new final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::_new;

  explicit _new(expression_arena::array_ptr<expression_ptr> children,
                source_code_span span) noexcept
      : expression(kind), span_(span), children_(children) {}

  int child_count() const noexcept override { return this->children_.size(); }

  expression_ptr child(int index) const noexcept override {
    return this->children_[index];
  }

  source_code_span span() const noexcept override { return this->span_; }

 private:
  source_code_span span_;
  expression_arena::array_ptr<expression_ptr> children_;
};
static_assert(expression_arena::is_allocatable<expression::_new>);

class expression::_template final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::_template;

  explicit _template(expression_arena::array_ptr<expression_ptr> children,
                     source_code_span span) noexcept
      : expression(kind), span_(span), children_(children) {}

  int child_count() const noexcept override { return this->children_.size(); }

  expression_ptr child(int index) const noexcept override {
    return this->children_[index];
  }

  source_code_span span() const noexcept override { return this->span_; }

 private:
  source_code_span span_;
  expression_arena::array_ptr<expression_ptr> children_;
};
static_assert(expression_arena::is_allocatable<expression::_template>);

class expression::array final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::array;

  explicit array(expression_arena::array_ptr<expression_ptr> children,
                 source_code_span span) noexcept
      : expression(kind), span_(span), children_(children) {}

  int child_count() const noexcept override { return this->children_.size(); }

  expression_ptr child(int index) const noexcept override {
    return this->children_[index];
  }

  source_code_span span() const noexcept override { return this->span_; }

 private:
  source_code_span span_;
  expression_arena::array_ptr<expression_ptr> children_;
};
static_assert(expression_arena::is_allocatable<expression::array>);

class expression::arrow_function_with_expression final : public expression {
 public:
  static constexpr expression_kind kind =
      expression_kind::arrow_function_with_expression;

  explicit arrow_function_with_expression(
      function_attributes attributes, expression_ptr body,
      const char *parameter_list_begin) noexcept
      : expression(kind),
        parameter_list_begin_(parameter_list_begin),
        function_attributes_(attributes),
        body_(body) {}

  explicit arrow_function_with_expression(
      function_attributes attributes,
      expression_arena::array_ptr<expression_ptr> parameters,
      expression_ptr body, const char *parameter_list_begin) noexcept
      : expression(kind),
        parameter_list_begin_(parameter_list_begin),
        function_attributes_(attributes),
        parameters_(parameters),
        body_(body) {
    if (!this->parameter_list_begin_) {
      assert(!this->parameters_.empty());
    }
  }

  int child_count() const noexcept override {
    return this->parameters_.size() + 1;
  }

  expression_ptr child(int index) const noexcept override {
    if (index == this->parameters_.size()) {
      return this->body_;
    } else {
      return this->parameters_[index];
    }
  }

  source_code_span span() const noexcept override {
    if (this->parameter_list_begin_) {
      return source_code_span(this->parameter_list_begin_,
                              this->body_->span().end());
    } else {
      return source_code_span(this->parameters_.front()->span().begin(),
                              this->body_->span().end());
    }
  }

  function_attributes attributes() const noexcept override {
    return this->function_attributes_;
  }

 private:
  const char *parameter_list_begin_;
  function_attributes function_attributes_;
  expression_arena::array_ptr<expression_ptr> parameters_;
  expression_ptr body_;
};
static_assert(expression_arena::is_allocatable<
              expression::arrow_function_with_expression>);

class expression::arrow_function_with_statements final : public expression {
 public:
  static constexpr expression_kind kind =
      expression_kind::arrow_function_with_statements;

  explicit arrow_function_with_statements(
      function_attributes attributes,
      expression_arena::buffering_visitor_ptr child_visits,
      const char *parameter_list_begin, const char *span_end) noexcept
      : expression(kind),
        function_attributes_(attributes),
        parameter_list_begin_(parameter_list_begin),
        span_end_(span_end),
        child_visits_(child_visits) {
    assert(this->parameter_list_begin_);
  }

  explicit arrow_function_with_statements(
      function_attributes attributes,
      expression_arena::array_ptr<expression_ptr> parameters,
      expression_arena::buffering_visitor_ptr child_visits,
      const char *parameter_list_begin, const char *span_end) noexcept
      : expression(kind),
        function_attributes_(attributes),
        parameter_list_begin_(parameter_list_begin),
        span_end_(span_end),
        child_visits_(child_visits),
        children_(parameters) {
    if (!this->parameter_list_begin_) {
      assert(!this->children_.empty());
    }
  }

  int child_count() const noexcept override { return this->children_.size(); }

  expression_ptr child(int index) const noexcept override {
    return this->children_[index];
  }

  source_code_span span() const noexcept override {
    if (this->parameter_list_begin_) {
      return source_code_span(this->parameter_list_begin_, this->span_end_);
    } else {
      return source_code_span(this->children_.front()->span().begin(),
                              span_end_);
    }
  }

  function_attributes attributes() const noexcept override {
    return this->function_attributes_;
  }

 protected:
  expression_arena::buffering_visitor_ptr
  take_child_visits() noexcept override {
    return std::exchange(this->child_visits_, nullptr);
  }

 private:
  function_attributes function_attributes_;
  const char *parameter_list_begin_;
  const char *span_end_;
  expression_arena::buffering_visitor_ptr child_visits_;
  expression_arena::array_ptr<expression_ptr> children_;
};
static_assert(expression_arena::is_allocatable<
              expression::arrow_function_with_statements>);

class expression::assignment final : public expression {
 public:
  explicit assignment(expression_kind kind, expression_ptr lhs,
                      expression_ptr rhs) noexcept
      : expression(kind), children_{lhs, rhs} {
    assert(kind == expression_kind::assignment ||
           kind == expression_kind::compound_assignment);
  }

  int child_count() const noexcept override {
    return narrow_cast<int>(this->children_.size());
  }

  expression_ptr child(int index) const noexcept override {
    assert(index >= 0);
    assert(index < static_cast<int>(this->children_.size()));
    return this->children_[narrow_cast<unsigned>(index)];
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->children_.front()->span().begin(),
                            this->children_.back()->span().end());
  }

 private:
  std::array<expression_ptr, 2> children_;
};
static_assert(expression_arena::is_allocatable<expression::assignment>);

class expression::await final
    : public expression::expression_with_prefix_operator_base {
 public:
  static constexpr expression_kind kind = expression_kind::await;

  explicit await(expression_ptr child, source_code_span operator_span) noexcept
      : expression::expression_with_prefix_operator_base(kind, child,
                                                         operator_span) {}
};
static_assert(expression_arena::is_allocatable<expression::await>);

class expression::binary_operator final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::binary_operator;

  explicit binary_operator(
      expression_arena::array_ptr<expression_ptr> children) noexcept
      : expression(kind), children_(children) {}

  int child_count() const noexcept override { return this->children_.size(); }

  expression_ptr child(int index) const noexcept override {
    return this->children_[index];
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->children_.front()->span().begin(),
                            this->children_.back()->span().end());
  }

 private:
  expression_arena::array_ptr<expression_ptr> children_;
};
static_assert(expression_arena::is_allocatable<expression::binary_operator>);

class expression::call final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::call;

  explicit call(expression_arena::array_ptr<expression_ptr> children,
                source_code_span span) noexcept
      : expression(kind),
        call_right_paren_end_(span.end()),
        children_(children) {}

  int child_count() const noexcept override { return this->children_.size(); }

  expression_ptr child(int index) const noexcept override {
    return this->children_[index];
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->children_.front()->span().begin(),
                            this->call_right_paren_end_);
  }

 private:
  const char *call_right_paren_end_;
  expression_arena::array_ptr<expression_ptr> children_;
};
static_assert(expression_arena::is_allocatable<expression::call>);

class expression::conditional final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::conditional;

  explicit conditional(expression_ptr condition, expression_ptr true_branch,
                       expression_ptr false_branch) noexcept
      : expression(kind), children_{condition, true_branch, false_branch} {}

  int child_count() const noexcept override {
    return narrow_cast<int>(this->children_.size());
  }

  expression_ptr child(int index) const noexcept override {
    assert(index >= 0);
    assert(index < static_cast<int>(this->children_.size()));
    return this->children_[narrow_cast<unsigned>(index)];
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->children_.front()->span().begin(),
                            this->children_.back()->span().end());
  }

 private:
  std::array<expression_ptr, 3> children_;
};
static_assert(expression_arena::is_allocatable<expression::conditional>);

class expression::dot final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::dot;

  explicit dot(expression_ptr lhs, identifier rhs) noexcept
      : expression(kind), variable_identifier_(rhs), child_(lhs) {}

  int child_count() const noexcept override { return 1; }

  expression_ptr child([[maybe_unused]] int index) const noexcept override {
    assert(index == 0);
    return this->child_;
  }

  identifier variable_identifier() const noexcept override {
    return this->variable_identifier_;
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->child_0()->span().begin(),
                            this->variable_identifier_.span().end());
  }

 private:
  identifier variable_identifier_;
  expression_ptr child_;
};
static_assert(expression_arena::is_allocatable<expression::dot>);

class expression::function final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::function;

  explicit function(function_attributes attributes,
                    expression_arena::buffering_visitor_ptr child_visits,
                    source_code_span span) noexcept
      : expression(kind),
        function_attributes_(attributes),
        child_visits_(child_visits),
        span_(span) {}

  source_code_span span() const noexcept override { return this->span_; }

  function_attributes attributes() const noexcept override {
    return this->function_attributes_;
  }

 protected:
  expression_arena::buffering_visitor_ptr
  take_child_visits() noexcept override {
    return std::exchange(this->child_visits_, nullptr);
  }

 private:
  function_attributes function_attributes_;
  expression_arena::buffering_visitor_ptr child_visits_;
  source_code_span span_;
};
static_assert(expression_arena::is_allocatable<expression::function>);

class expression::import final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::import;

  explicit import(source_code_span span) noexcept
      : expression(kind), span_(span) {}

  source_code_span span() const noexcept override { return this->span_; }

 private:
  source_code_span span_;
};
static_assert(expression_arena::is_allocatable<expression::import>);

class expression::index final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::index;

  explicit index(expression_ptr container, expression_ptr subscript,
                 const char *subscript_end) noexcept
      : expression(kind),
        index_subscript_end_(subscript_end),
        children_{container, subscript} {}

  int child_count() const noexcept override {
    return narrow_cast<int>(this->children_.size());
  }

  expression_ptr child(int index) const noexcept override {
    assert(index >= 0);
    assert(index < static_cast<int>(this->children_.size()));
    return this->children_[narrow_cast<unsigned>(index)];
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->child_0()->span().begin(),
                            this->index_subscript_end_);
  }

 private:
  const char *index_subscript_end_;
  std::array<expression_ptr, 2> children_;
};
static_assert(expression_arena::is_allocatable<expression::index>);

class expression::literal final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::literal;

  explicit literal(source_code_span span) noexcept
      : expression(kind), span_(span) {}

  source_code_span span() const noexcept override { return this->span_; }

 private:
  source_code_span span_;
};
static_assert(expression_arena::is_allocatable<expression::literal>);

class expression::named_function final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::named_function;

  explicit named_function(function_attributes attributes, identifier name,
                          expression_arena::buffering_visitor_ptr child_visits,
                          source_code_span span) noexcept
      : expression(kind),
        function_attributes_(attributes),
        child_visits_(child_visits),
        variable_identifier_(name),
        span_(span) {}

  identifier variable_identifier() const noexcept override {
    return this->variable_identifier_;
  }

  source_code_span span() const noexcept override { return this->span_; }

  function_attributes attributes() const noexcept override {
    return this->function_attributes_;
  }

 protected:
  expression_arena::buffering_visitor_ptr
  take_child_visits() noexcept override {
    return std::exchange(this->child_visits_, nullptr);
  }

 private:
  function_attributes function_attributes_;
  expression_arena::buffering_visitor_ptr child_visits_;
  identifier variable_identifier_;
  source_code_span span_;
};
static_assert(expression_arena::is_allocatable<expression::named_function>);

class expression::object final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::object;

  explicit object(
      expression_arena::array_ptr<object_property_value_pair> entries,
      source_code_span span) noexcept
      : expression(kind), span_(span), entries_(entries) {}

  int object_entry_count() const noexcept override {
    return this->entries_.size();
  }

  object_property_value_pair object_entry(int index) const noexcept override {
    return this->entries_[index];
  }

  source_code_span span() const noexcept override { return this->span_; }

 private:
  source_code_span span_;
  expression_arena::array_ptr<object_property_value_pair> entries_;
};
static_assert(expression_arena::is_allocatable<expression::object>);

class expression::rw_unary_prefix final
    : public expression::expression_with_prefix_operator_base {
 public:
  static constexpr expression_kind kind = expression_kind::rw_unary_prefix;

  explicit rw_unary_prefix(expression_ptr child,
                           source_code_span operator_span) noexcept
      : expression::expression_with_prefix_operator_base(kind, child,
                                                         operator_span) {}
};
static_assert(expression_arena::is_allocatable<expression::rw_unary_prefix>);

class expression::rw_unary_suffix final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::rw_unary_suffix;

  explicit rw_unary_suffix(expression_ptr child,
                           source_code_span operator_span) noexcept
      : expression(kind),
        unary_operator_end_(operator_span.end()),
        child_(child) {}

  int child_count() const noexcept override { return 1; }

  expression_ptr child([[maybe_unused]] int index) const noexcept override {
    assert(index == 0);
    return this->child_;
  }

  source_code_span span() const noexcept override {
    return source_code_span(this->child_->span().begin(),
                            this->unary_operator_end_);
  }

 private:
  const char *unary_operator_end_;
  expression_ptr child_;
};
static_assert(expression_arena::is_allocatable<expression::rw_unary_suffix>);

class expression::spread final
    : public expression::expression_with_prefix_operator_base {
 public:
  static constexpr expression_kind kind = expression_kind::spread;

  explicit spread(expression_ptr child, source_code_span operator_span) noexcept
      : expression::expression_with_prefix_operator_base(kind, child,
                                                         operator_span) {}
};
static_assert(expression_arena::is_allocatable<expression::spread>);

class expression::super final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::super;

  explicit super(source_code_span span) noexcept
      : expression(kind), span_(span) {}

  source_code_span span() const noexcept override { return this->span_; }

 private:
  source_code_span span_;
};
static_assert(expression_arena::is_allocatable<expression::super>);

class expression::unary_operator final
    : public expression::expression_with_prefix_operator_base {
 public:
  static constexpr expression_kind kind = expression_kind::unary_operator;

  explicit unary_operator(expression_ptr child,
                          source_code_span operator_span) noexcept
      : expression::expression_with_prefix_operator_base(kind, child,
                                                         operator_span) {}
};
static_assert(expression_arena::is_allocatable<expression::unary_operator>);

class expression::variable final : public expression {
 public:
  static constexpr expression_kind kind = expression_kind::variable;

  explicit variable(identifier variable_identifier) noexcept
      : expression(kind), variable_identifier_(variable_identifier) {}

  identifier variable_identifier() const noexcept override {
    return this->variable_identifier_;
  }

  source_code_span span() const noexcept override {
    return this->variable_identifier_.span();
  }

 private:
  identifier variable_identifier_;
};
static_assert(expression_arena::is_allocatable<expression::variable>);
}  // namespace quick_lint_js

QLJS_WARNING_POP

#undef QLJS_UNEXPECTED_EXPRESSION_KIND

#endif
