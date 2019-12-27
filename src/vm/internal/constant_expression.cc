#include "../public/constant_expression.h"

#include <glog/logging.h>

#include "../public/value.h"
#include "../public/vm.h"

namespace afc {
namespace vm {

namespace {

class ConstantExpression : public Expression {
 public:
  ConstantExpression(unique_ptr<Value> value) : value_(std::move(value)) {
    CHECK(value_ != nullptr);
  }

  std::vector<VMType> Types() { return {value_->type}; }
  std::unordered_set<VMType> ReturnTypes() const override { return {}; }

  void Evaluate(Trampoline* trampoline, const VMType& type) {
    CHECK(type == value_->type);
    DVLOG(5) << "Evaluating constant value: " << *value_;
    trampoline->Continue(std::make_unique<Value>(*value_));
  }

  std::unique_ptr<Expression> Clone() override {
    return std::make_unique<ConstantExpression>(
        std::make_unique<Value>(*value_));
  }

 private:
  const std::unique_ptr<Value> value_;
};

}  // namespace

std::unique_ptr<Expression> NewVoidExpression() {
  return NewConstantExpression(Value::NewVoid());
}

std::unique_ptr<Expression> NewConstantExpression(
    std::unique_ptr<Value> value) {
  CHECK(value != nullptr);
  return std::make_unique<ConstantExpression>(std::move(value));
}

}  // namespace vm
}  // namespace afc
