#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <quicklint-js/error.h>
#include <quicklint-js/lex.h>
#include <quicklint-js/lint.h>
#include <quicklint-js/location.h>
#include <quicklint-js/options.h>
#include <quicklint-js/parse.h>
#include <string>

namespace quicklint_js {
namespace {
void process_file(const char *path, bool print_parser_visits);
std::string read_file(const char *path);
}  // namespace
}  // namespace quicklint_js

int main(int argc, char **argv) {
  quicklint_js::options o = quicklint_js::parse_options(argc, argv);
  if (!o.error_unrecognized_options.empty()) {
    for (const auto &option : o.error_unrecognized_options) {
      std::cerr << "error: unrecognized option: " << option << '\n';
    }
    return 1;
  }
  if (o.files_to_lint.empty()) {
    std::cerr << "error: expected file name\n";
    return 1;
  }

  for (const char *file_to_lint : o.files_to_lint) {
    quicklint_js::process_file(file_to_lint, o.print_parser_visits);
  }

  return 0;
}

namespace quicklint_js {
namespace {
class debug_error_reporter : public error_reporter {
 public:
  explicit debug_error_reporter(const char *input) noexcept : locator_(input) {}

  void report_error_invalid_binding_in_let_statement(
      source_code_span where) override {
    log_location(where);
    std::cerr << "error: invalid binding in let statement\n";
  }

  void report_error_let_with_no_bindings(source_code_span where) override {
    log_location(where);
    std::cerr << "error: let with no bindings\n";
  }

  void report_error_missing_oprand_for_operator(
      source_code_span where) override {
    log_location(where);
    std::cerr << "error: missing oprand for operator\n";
  }

  void report_error_stray_comma_in_let_statement(
      source_code_span where) override {
    log_location(where);
    std::cerr << "error: stray comma in let statement\n";
  }

  void report_error_unclosed_block_comment(source_code_span where) override {
    log_location(where);
    std::cerr << "error: unclosed block comment\n";
  }

  void report_error_unclosed_string_literal(source_code_span where) override {
    log_location(where);
    std::cerr << "error: unclosed string literal\n";
  }

  void report_error_unexpected_identifier(source_code_span where) override {
    log_location(where);
    std::cerr << "error: unexpected identifier\n";
  }

  void report_error_unmatched_parenthesis(source_code_span where) override {
    log_location(where);
    std::cerr << "error: unmatched parenthesis\n";
  }

  void report_error_variable_used_before_declaration(identifier name) override {
    log_location(name);
    std::cerr << "error: variable used before declaration\n";
  }

 private:
  void log_location(identifier i) const { log_location(i.span()); }

  void log_location(source_code_span span) const {
    source_range r = this->locator_.range(span);
    source_position p = r.begin();
    std::cerr << p.line_number << ":" << p.column_number << ": ";
  }

  locator locator_;
};

class debug_visitor {
 public:
  void visit_enter_function_scope() { std::cerr << "entered function scope\n"; }

  void visit_exit_function_scope() { std::cerr << "exited function scope\n"; }

  void visit_variable_declaration(identifier name) {
    std::cerr << "variable declaration: " << name.string_view() << '\n';
  }

  void visit_variable_use(identifier name) {
    std::cerr << "variable use: " << name.string_view() << '\n';
  }
};

template <class Visitor1, class Visitor2>
class multi_visitor {
 public:
  explicit multi_visitor(Visitor1 *visitor_1, Visitor2 *visitor_2) noexcept
      : visitor_1_(visitor_1), visitor_2_(visitor_2) {}

  void visit_enter_function_scope() {
    this->visitor_1_->visit_enter_function_scope();
    this->visitor_2_->visit_enter_function_scope();
  }

  void visit_exit_function_scope() {
    this->visitor_1_->visit_exit_function_scope();
    this->visitor_2_->visit_exit_function_scope();
  }

  void visit_variable_declaration(identifier name) {
    this->visitor_1_->visit_variable_declaration(name);
    this->visitor_2_->visit_variable_declaration(name);
  }

  void visit_variable_use(identifier name) {
    this->visitor_1_->visit_variable_use(name);
    this->visitor_2_->visit_variable_use(name);
  }

 private:
  Visitor1 *visitor_1_;
  Visitor2 *visitor_2_;
};

void process_file(const char *path, bool print_parser_visits) {
  std::string source = read_file(path);
  debug_error_reporter error_reporter(source.c_str());
  parser p(source.c_str(), &error_reporter);
  linter l(&error_reporter);
  if (print_parser_visits) {
    debug_visitor logger;
    multi_visitor visitor(&logger, &l);
    p.parse_module(visitor);
  } else {
    p.parse_module(l);
  }
}

std::string read_file(const char *path) {
  FILE *file = std::fopen(path, "rb");
  if (!file) {
    std::cerr << "error: failed to open " << path << ": "
              << std::strerror(errno) << '\n';
    exit(1);
  }

  if (std::fseek(file, 0, SEEK_END) == -1) {
    std::cerr << "error: failed to seek to end of " << path << ": "
              << std::strerror(errno) << '\n';
    exit(1);
  }

  long file_size = std::ftell(file);
  if (file_size == -1) {
    std::cerr << "error: failed to get size of " << path << ": "
              << std::strerror(errno) << '\n';
    exit(1);
  }

  if (std::fseek(file, 0, SEEK_SET) == -1) {
    std::cerr << "error: failed to seek to beginning of " << path << ": "
              << std::strerror(errno) << '\n';
    exit(1);
  }

  std::string contents;
  contents.resize(file_size);
  std::size_t read_size = std::fread(contents.data(), 1, file_size, file);
  contents.resize(read_size);

  if (std::fclose(file) == -1) {
    std::cerr << "error: failed to close " << path << ": "
              << std::strerror(errno) << '\n';
    exit(1);
  }

  return contents;
}
}  // namespace
}  // namespace quicklint_js