#include "../public/environment.h"

#include <glog/logging.h>

#include <map>
#include <set>

#include "../public/callbacks.h"
#include "../public/set.h"
#include "../public/types.h"
#include "../public/value.h"
#include "../public/vector.h"
#include "string.h"

namespace afc {
namespace vm {

namespace {

template <>
const VMType VMTypeMapper<std::vector<int>*>::vmtype =
    VMType::ObjectType(L"VectorInt");

template <>
const VMType VMTypeMapper<std::set<int>*>::vmtype =
    VMType::ObjectType(L"SetInt");

std::shared_ptr<Environment> BuildDefaultEnvironment() {
  auto environment = std::make_shared<Environment>();
  RegisterStringType(environment.get());

  auto bool_type = std::make_unique<ObjectType>(VMType::Bool());
  bool_type->AddField(L"tostring",
                      NewCallback(std::function<wstring(bool)>(
                          [](bool v) { return v ? L"true" : L"false"; })));
  environment->DefineType(L"bool", std::move(bool_type));

  auto int_type = std::make_unique<ObjectType>(VMType::Integer());
  int_type->AddField(L"tostring",
                     NewCallback(std::function<std::wstring(int)>(
                         [](int value) { return std::to_wstring(value); })));
  environment->DefineType(L"int", std::move(int_type));

  auto double_type = std::make_unique<ObjectType>(VMType::Double());
  double_type->AddField(
      L"tostring", NewCallback(std::function<std::wstring(double)>(
                       [](double value) { return std::to_wstring(value); })));
  double_type->AddField(
      L"round", NewCallback(std::function<int(double)>(
                    [](double value) { return static_cast<int>(value); })));
  environment->DefineType(L"double", std::move(double_type));

  VMTypeMapper<std::vector<int>*>::Export(environment.get());
  VMTypeMapper<std::set<int>*>::Export(environment.get());
  return environment;
}

}  // namespace

void Environment::Clear() {
  object_types_.clear();
  table_.clear();
}

/* static */ const std::shared_ptr<Environment>& Environment::GetDefault() {
  static std::shared_ptr<Environment> environment = BuildDefaultEnvironment();
  return environment;
}

const ObjectType* Environment::LookupObjectType(const wstring& symbol) {
  auto it = object_types_.find(symbol);
  if (it != object_types_.end()) {
    return it->second.get();
  }
  if (parent_environment_ != nullptr) {
    return parent_environment_->LookupObjectType(symbol);
  }
  return nullptr;
}

const VMType* Environment::LookupType(const wstring& symbol) {
  if (symbol == L"void") {
    return &VMType::Void();
  } else if (symbol == L"bool") {
    return &VMType::Bool();
  } else if (symbol == L"int") {
    return &VMType::Integer();
  } else if (symbol == L"string") {
    return &VMType::String();
  }

  auto object_type = LookupObjectType(symbol);
  return object_type == nullptr ? nullptr : &object_type->type();
}

Environment::Environment() = default;

Environment::Environment(std::shared_ptr<Environment> parent_environment)
    : parent_environment_(std::move(parent_environment)) {}

void Environment::DefineType(const wstring& name,
                             unique_ptr<ObjectType> value) {
  auto it = object_types_.insert(make_pair(name, nullptr));
  it.first->second = std::move(value);
}

Value* Environment::Lookup(const wstring& symbol, VMType expected_type) {
  std::vector<Value*> values;
  PolyLookup(symbol, &values);
  for (auto& value : values) {
    if (value->type == expected_type) {
      return value;
    }
  }
  return nullptr;
}

void Environment::PolyLookup(const wstring& symbol,
                             std::vector<Value*>* output) {
  if (auto it = table_.find(symbol); it != table_.end()) {
    for (auto& entry : it->second) {
      output->push_back(entry.second.get());
    }
  }
  if (parent_environment_ != nullptr) {
    parent_environment_->PolyLookup(symbol, output);
  }
}

void Environment::CaseInsensitiveLookup(const wstring& symbol,
                                        std::vector<Value*>* output) {
  for (auto& item : table_) {
    if (wcscasecmp(item.first.c_str(), symbol.c_str()) == 0) {
      for (auto& entry : item.second) {
        output->push_back(entry.second.get());
      }
    }
  }
  if (parent_environment_ != nullptr) {
    parent_environment_->CaseInsensitiveLookup(symbol, output);
  }
}

void Environment::Define(const wstring& symbol, unique_ptr<Value> value) {
  table_[symbol][value->type] = std::move(value);
}

void Environment::Assign(const wstring& symbol, unique_ptr<Value> value) {
  auto it = table_.find(symbol);
  if (it == table_.end()) {
    // TODO: Show the symbol.
    CHECK(parent_environment_ != nullptr)
        << "Environment::parent_environment_ is nullptr while trying to "
           "assign "
        << "a new value to a symbol `"
        << "..."
        << "`. This likely means that "
        << "the symbol is undefined (which the caller should have validated "
           "as "
        << "part of the compilation process).";
    parent_environment_->Assign(symbol, std::move(value));
    return;
  }
  it->second[value->type] = std::move(value);
}

void Environment::ForEachType(
    std::function<void(const wstring&, ObjectType*)> callback) {
  if (parent_environment_ != nullptr) {
    parent_environment_->ForEachType(callback);
  }
  for (auto& entry : object_types_) {
    callback(entry.first, entry.second.get());
  }
}

void Environment::ForEach(
    std::function<void(const wstring&, Value*)> callback) {
  if (parent_environment_ != nullptr) {
    parent_environment_->ForEach(callback);
  }
  for (auto& symbol_entry : table_) {
    for (auto& type_entry : symbol_entry.second) {
      callback(symbol_entry.first, type_entry.second.get());
    }
  }
}

}  // namespace vm
}  // namespace afc
