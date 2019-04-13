#include "src/map_mode.h"

#include <glog/logging.h>

#include <memory>
#include <set>

#include "src/command.h"
#include "src/help_command.h"
#include "src/vm/public/constant_expression.h"
#include "src/vm/public/function_call.h"
#include "src/vm/public/types.h"
#include "src/vm/public/value.h"
#include "src/vm/public/vm.h"

namespace afc {
namespace editor {

using vm::Expression;
using vm::Value;
using vm::VMType;

namespace {
class CommandFromFunction : public Command {
 public:
  CommandFromFunction(std::function<void()> callback, wstring description)
      : callback_(std::move(callback)), description_(std::move(description)) {
    CHECK(callback_ != nullptr);
  }

  std::wstring Description() const override { return description_; }
  std::wstring Category() const override {
    return L"C++ Functions (Extensions)";
  }

  void ProcessInput(wint_t, EditorState*) override { callback_(); }

 private:
  const std::function<void()> callback_;
  const wstring description_;
};

}  // namespace

class EditorState;
MapModeCommands::MapModeCommands()
    : commands_({std::make_shared<map<wstring, std::unique_ptr<Command>>>()}) {
  Add(L"?", NewHelpCommand(this, L"command mode"));
}

std::unique_ptr<MapModeCommands> MapModeCommands::NewChild() {
  auto output = std::make_unique<MapModeCommands>();
  output->commands_ = commands_;
  output->commands_.push_front(
      std::make_shared<map<wstring, std::unique_ptr<Command>>>());

  // Override the parent's help command, so that bindings added to the child are
  // visible.
  output->Add(L"?", NewHelpCommand(output.get(), L"command mode"));
  return std::move(output);
}

std::map<wstring, std::map<wstring, Command*>> MapModeCommands::Coallesce()
    const {
  std::map<wstring, std::map<wstring, Command*>> output;
  std::set<wstring> already_seen;  // Avoid showing unreachable commands.
  for (const auto& node : commands_) {
    for (const auto& it : *node) {
      if (already_seen.insert(it.first).second) {
        output[it.second->Category()][it.first] = it.second.get();
      }
    }
  }
  return output;
}

void MapModeCommands::Add(wstring name, std::unique_ptr<Command> value) {
  CHECK(value != nullptr);
  commands_.front()->insert({name, std::move(value)});
}

void MapModeCommands::Add(wstring name, wstring description,
                          std::unique_ptr<Value> value,
                          vm::Environment* environment) {
  CHECK(value != nullptr);
  CHECK_EQ(value->type.type, VMType::FUNCTION);
  CHECK(value->type.type_arguments == std::vector<VMType>({VMType::Void()}));
  // TODO: Make a unique_ptr (once capture of unique_ptr is feasible).
  std::shared_ptr<vm::Expression> expression =
      NewFunctionCall(NewConstantExpression(std::move(value)), {});
  Add(name, std::make_unique<CommandFromFunction>(
                [expression, environment]() {
                  LOG(INFO) << "Evaluating expression from Value::Ptr...";
                  Evaluate(expression.get(), environment,
                           [expression](Value::Ptr) {
                             LOG(INFO) << "Done evaluating.";
                           });
                },
                description));
}

void MapModeCommands::Add(wstring name, std::function<void()> callback,
                          wstring description) {
  Add(name, std::make_unique<CommandFromFunction>(std::move(callback),
                                                  std::move(description)));
}

MapMode::MapMode(std::shared_ptr<MapModeCommands> commands)
    : commands_(std::move(commands)) {}

void MapMode::ProcessInput(wint_t c, EditorState* editor_state) {
  current_input_.push_back(c);

  bool reset_input = true;
  for (const auto& node : commands_->commands_) {
    auto it = node->lower_bound(current_input_);
    if (it != node->end() &&
        std::equal(current_input_.begin(), current_input_.end(),
                   it->first.begin())) {
      if (current_input_ == it->first) {
        CHECK(it->second);
        current_input_ = L"";
        it->second->ProcessInput(c, editor_state);
        return;
      }
      reset_input = false;
    }
  }

  if (reset_input) {
    current_input_ = L"";
  }
}

}  // namespace editor
}  // namespace afc
