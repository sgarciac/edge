#ifndef __AFC_EDITOR_VARIABLES_H__
#define __AFC_EDITOR_VARIABLES_H__

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace afc {
namespace editor {

using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

template <typename T>
class EdgeStruct;

template <typename T>
struct EdgeVariable {
 public:
  const string& name() const { return name_; }
  const string& description() const { return description_; }
  const T& default_value() const { return default_value_; }
  const size_t position() const { return position_; }

 private:
  // Instantiate it through EdgeStruct::AddVariable.
  EdgeVariable(const string& name,
               const string& description,
               const T& default_value,
               size_t position)
      : name_(name),
        description_(description),
        default_value_(default_value),
        position_(position) {}

  string name_;
  string description_;
  T default_value_;
  size_t position_;

  friend class EdgeStruct<T>;
};

template <typename T>
class EdgeStructInstance {
 public:
  T Get(const EdgeVariable<T>* variable) const;
  void Set(const EdgeVariable<T>* variable, const T& value);

 private:
  // Instantiate it through EdgeStruct::NewInstance.
  EdgeStructInstance() {}

  vector<T> values_;

  friend class EdgeStruct<T>;
};

template <typename T>
class EdgeStruct {
 public:
  EdgeVariable<T>* AddVariable(
      const string& name, const string& description, const T& default_value);

  EdgeStructInstance<T> NewInstance() {
    EdgeStructInstance<T> instance;
    instance.values_.resize(variables_.size());
    for (const auto& v : variables_) {
      instance.values_[v.second->position()] = v.second->default_value();
    }
    return instance;
  }

  const EdgeVariable<T>* find_variable(const string& name) {
    auto it = variables_.find(name);
    return it == variables_.end() ? nullptr : it->second.get();
  }

 private:
  map<string, unique_ptr<EdgeVariable<T>>> variables_;
};

template <typename T>
T EdgeStructInstance<T>::Get(const EdgeVariable<T>* variable) const {
  return values_[variable->position()];
}

template <typename T>
void EdgeStructInstance<T>::Set(
    const EdgeVariable<T>* variable, const T& value) {
  values_[variable->position()] = value;
}

template <typename T>
EdgeVariable<T>* EdgeStruct<T>::AddVariable(
    const string& name, const string& description, const T& default_value) {
  auto it = variables_.insert(make_pair(name, new EdgeVariable<T>(
      name, description, default_value, variables_.size())));
  return it.first->second.get();
}

}  // namespace editor
}  // namespace afc

#endif
