#include "../public/value.h"

#include "../public/vm.h"
#include "wstring.h"

namespace afc {
namespace vm {

/* static */ std::unique_ptr<Value> Value::NewVoid() {
  return std::make_unique<Value>(VMType::VM_VOID);
}

/* static */ std::unique_ptr<Value> Value::NewBool(bool value) {
  auto output = std::make_unique<Value>(VMType::Bool());
  output->boolean = value;
  return output;
}

/* static */ std::unique_ptr<Value> Value::NewInteger(int value) {
  auto output = std::make_unique<Value>(VMType::Integer());
  output->integer = value;
  return output;
}

/* static */ std::unique_ptr<Value> Value::NewDouble(double value) {
  auto output = std::make_unique<Value>(VMType::Double());
  output->double_value = value;
  return output;
}

/* static */ std::unique_ptr<Value> Value::NewString(wstring value) {
  auto output = std::make_unique<Value>(VMType::String());
  output->str = std::move(value);
  return output;
}

/* static */ std::unique_ptr<Value> Value::NewObject(
    const wstring& name, const shared_ptr<void>& value) {
  auto output = std::make_unique<Value>(VMType::ObjectType(name));
  output->user_value = value;
  return output;
}

/* static */ std::unique_ptr<Value> Value::NewFunction(
    std::vector<VMType> arguments, Value::Callback callback) {
  auto output = std::make_unique<Value>(VMType::FUNCTION);
  output->type.type_arguments = std::move(arguments);
  output->callback = std::move(callback);
  return output;
}

/* static */ std::unique_ptr<Value> Value::NewFunction(
    std::vector<VMType> arguments,
    std::function<Value::Ptr(std::vector<Value::Ptr>)> callback) {
  return NewFunction(arguments, [callback](std::vector<Ptr> args, Trampoline*) {
    return futures::Past(EvaluationOutput::New(callback(std::move(args))));
  });
}

std::ostream& operator<<(std::ostream& os, const Value& value) {
  if (value.IsInteger()) {
    os << value.integer;
  } else if (value.IsString()) {
    os << '"' << value.str << '"';
  } else if (value.IsBool()) {
    os << value.boolean;
  } else if (value.IsDouble()) {
    os << value.double_value;
  } else {
    os << value.type.ToString();
  }
  return os;
}

}  // namespace vm
}  // namespace afc
