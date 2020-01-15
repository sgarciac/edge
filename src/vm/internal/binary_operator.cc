#include "binary_operator.h"

#include <glog/logging.h>

#include "../public/value.h"
#include "src/vm/internal/compilation.h"

namespace afc {
namespace vm {

BinaryOperator::BinaryOperator(
    unique_ptr<Expression> a, unique_ptr<Expression> b, const VMType type,
    function<void(const Value&, const Value&, Value*)> callback)
    : a_(std::move(a)), b_(std::move(b)), type_(type), operator_(callback) {
  CHECK(a_ != nullptr);
  CHECK(b_ != nullptr);
}

std::vector<VMType> BinaryOperator::Types() { return {type_}; }

std::unordered_set<VMType> BinaryOperator::ReturnTypes() const {
  // TODO(easy): Should take b into account. That means changing cpp.y to be
  // able to handle errors here.
  return a_->ReturnTypes();
}

futures::Value<EvaluationOutput> BinaryOperator::Evaluate(
    Trampoline* trampoline, const VMType& type) {
  CHECK(type_ == type);
  return futures::Transform(
      trampoline->Bounce(a_.get(), a_->Types()[0]),
      [b = b_, type = type_, op = operator_,
       trampoline](EvaluationOutput a_value) {
        return futures::ImmediateTransform(
            trampoline->Bounce(b.get(), b->Types()[0]),
            [a_value = std::make_shared<Value>(std::move(*a_value.value)), type,
             op](EvaluationOutput b_value) {
              auto output = std::make_unique<Value>(type);
              op(*a_value, *b_value.value, output.get());
              return EvaluationOutput::New(std::move(output));
            });
      });
}

std::unique_ptr<Expression> BinaryOperator::Clone() {
  return std::make_unique<BinaryOperator>(a_->Clone(), b_->Clone(), type_,
                                          operator_);
}

std::unique_ptr<Expression> NewBinaryExpression(
    Compilation* compilation, std::unique_ptr<Expression> a,
    std::unique_ptr<Expression> b,
    std::function<wstring(wstring, wstring)> str_operator,
    std::function<int(int, int)> int_operator,
    std::function<double(double, double)> double_operator,
    std::function<wstring(wstring, int)> str_int_operator) {
  if (a == nullptr || b == nullptr) {
    return nullptr;
  }

  if (str_operator != nullptr && a->IsString() && b->IsString()) {
    return std::make_unique<BinaryOperator>(
        std::move(a), std::move(b), VMType::String(),
        [str_operator](const Value& value_a, const Value& value_b,
                       Value* output) {
          output->str = str_operator(value_a.str, value_b.str);
        });
  }

  if (int_operator != nullptr && a->IsInteger() && b->IsInteger()) {
    return std::make_unique<BinaryOperator>(
        std::move(a), std::move(b), VMType::Integer(),
        [int_operator](const Value& value_a, const Value& value_b,
                       Value* output) {
          output->integer = int_operator(value_a.integer, value_b.integer);
        });
  }

  if (double_operator != nullptr && (a->IsInteger() || a->IsDouble()) &&
      (b->IsInteger() || b->IsDouble())) {
    return std::make_unique<BinaryOperator>(
        std::move(a), std::move(b), VMType::Double(),
        [double_operator](const Value& a, const Value& b, Value* output) {
          auto to_double = [](const Value& x) {
            if (x.type.type == VMType::VM_INTEGER) {
              return static_cast<double>(x.integer);
            } else if (x.type.type == VMType::VM_DOUBLE) {
              return x.double_value;
            } else {
              CHECK(false) << "Unexpected type: " << x.type;
              return 0.0;  // Silence warning: no return.
            }
          };
          output->double_value = double_operator(to_double(a), to_double(b));
        });
  }

  if (str_int_operator != nullptr && a->IsString() && b->IsInteger()) {
    return std::make_unique<BinaryOperator>(
        std::move(a), std::move(b), VMType::String(),
        [str_int_operator](const Value& value_a, const Value& value_b,
                           Value* output) {
          output->str = str_int_operator(value_a.str, value_b.integer);
        });
  }

  // TODO: Find a way to support this.
  compilation->errors.push_back(L"Unable to add types" /*: \"" +
                                a->type().ToString() + L"\" + \"" +
                                b->type().ToString() + L"\""*/);
  return nullptr;
}

}  // namespace vm
}  // namespace afc
