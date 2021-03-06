#ifndef __AFC_VM_PUBLIC_VECTOR_H__
#define __AFC_VM_PUBLIC_VECTOR_H__

#include <glog/logging.h>

#include <memory>
#include <type_traits>

#include "src/vm/public/value.h"
#include "src/vm/public/vm.h"

namespace afc {
namespace vm {

// Defines a vector type.
//
// To use it, define the vmtypes in your module:
//
//     template <>
//     const VMType VMTypeMapper<std::vector<MyType>*>::vmtype =
//         VMType::ObjectType(L"VectorMyType");
//
//     template <>
//     const VMType VMTypeMapper<std::unique_ptr<std::vector<MyType>>>::vmtype =
//         VMType::ObjectType(L"VectorMyType");
//
// Then initialize it in an environment:
//
//     VMTypeMapper<std::vector<MyType>*>::Export(&environment);
//
// You can then define a function that operates in vectors such as:
//
//    vm::NewCallback(
//        std::function<std::unique_ptr<std::vector<T>>(std::vector<T>*)>(...));
template <typename T>
struct VMTypeMapper<std::vector<T>*> {
  static std::vector<T>* get(Value* value) {
    return static_cast<std::vector<T>*>(value->user_value.get());
  }

  static const VMType vmtype;

  static void Export(Environment* environment) {
    auto vector_type = std::make_unique<ObjectType>(vmtype);

    auto name = vmtype.object_type;
    environment->Define(
        name, Value::NewFunction({VMType::ObjectType(vector_type.get())},
                                 [name](std::vector<Value::Ptr> args) {
                                   CHECK(args.empty());
                                   return Value::NewObject(
                                       name,
                                       std::make_shared<std::vector<T>>());
                                 }));

    vector_type->AddField(L"empty", vm::NewCallback([](std::vector<T>* v) {
                            return v->empty();
                          }));
    vector_type->AddField(L"size", vm::NewCallback([](std::vector<T>* v) {
                            return static_cast<int>(v->size());
                          }));
    vector_type->AddField(L"get", vm::NewCallback([](std::vector<T>* v, int i) {
                            CHECK_LT(static_cast<size_t>(i), v->size());
                            return v->at(i);
                          }));
    vector_type->AddField(L"erase",
                          vm::NewCallback([](std::vector<T>* v, int i) {
                            v->erase(v->begin() + i);
                          }));
    vector_type->AddField(
        L"push_back",
        vm::NewCallback([](std::vector<T>* v, T e) { v->emplace_back(e); }));

    environment->DefineType(name, std::move(vector_type));
  }
};

// Allow safer construction than with VMTypeMapper<std::vector<T>>::New.
template <typename T>
struct VMTypeMapper<std::unique_ptr<std::vector<T>>> {
  static Value::Ptr New(std::unique_ptr<std::vector<T>> value) {
    std::shared_ptr<void> void_ptr(value.release(), [](void* value) {
      delete static_cast<std::vector<T>*>(value);
    });
    return Value::NewObject(vmtype.object_type, void_ptr);
  }

  static const VMType vmtype;
};

}  // namespace vm
}  // namespace afc

#endif  // __AFC_VM_PUBLIC_VECTOR_H__
