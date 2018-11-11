#include "if_expression.h"

#include <cassert>

#include "../public/value.h"
#include "../internal/compilation.h"
#include "../internal/evaluation.h"

namespace afc {
namespace vm {

namespace {

class IfExpression : public Expression {
 public:
  IfExpression(
      unique_ptr<Expression> cond, unique_ptr<Expression> true_case,
      unique_ptr<Expression> false_case)
      : cond_(std::move(cond)),
        true_case_(std::move(true_case)),
        false_case_(std::move(false_case)) {
    assert(cond_ != nullptr);
    assert(true_case_ != nullptr);
    assert(false_case_ != nullptr);
  }

  const VMType& type() {
    return true_case_->type();
  }

  void Evaluate(Trampoline* trampoline) {
    trampoline->Bounce(
        cond_.get(),
        [this](std::unique_ptr<Value> result, Trampoline* trampoline) {
          (result->boolean ? true_case_ : false_case_)->Evaluate(trampoline);
        });
  }

 private:
  const std::unique_ptr<Expression> cond_;
  const std::unique_ptr<Expression> true_case_;
  const std::unique_ptr<Expression> false_case_;
};

}

unique_ptr<Expression> NewIfExpression(
    Compilation* compilation,
    unique_ptr<Expression> condition,
    unique_ptr<Expression> true_case,
    unique_ptr<Expression> false_case) {
  if (condition == nullptr || true_case == nullptr || false_case == nullptr) {
    return nullptr;
  }

  if (condition->type().type != VMType::VM_BOOLEAN) {
    compilation->errors.push_back(
        L"Expected bool value for condition of \"if\" expression but found \""
        + condition->type().ToString() + L"\".");
    return nullptr;
  }

  if (!(true_case->type() == false_case->type())) {
    compilation->errors.push_back(
        L"Type mismatch between branches of conditional expression: \""
        + true_case->type().ToString() + L"\" and \""
        + false_case->type().ToString() + L"\"");
    return nullptr;
  }

  return unique_ptr<Expression>(new IfExpression(
      std::move(condition), std::move(true_case), std::move(false_case)));
}

}  // namespace afc
}  // namespace vm
