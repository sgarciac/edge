#include "src/navigation_buffer.h"

#include "src/buffer_variables.h"
#include "src/char_buffer.h"
#include "src/command.h"
#include "src/dirname.h"
#include "src/editor.h"
#include "src/file_link_mode.h"
#include "src/insert_mode.h"
#include "src/lazy_string_append.h"
#include "src/lazy_string_trim.h"
#include "src/line_prompt_mode.h"
#include "src/parse_tree.h"
#include "src/screen.h"
#include "src/send_end_of_file_command.h"
#include "src/wstring.h"

namespace afc {
namespace editor {
namespace {
const wstring kDepthSymbol = L"navigation_buffer_depth";

void AdjustLastLine(OpenBuffer* buffer, std::shared_ptr<OpenBuffer> link_to,
                    LineColumn position) {
  auto line_environment = buffer->contents()->back()->environment();
  line_environment->Define(L"buffer", Value::NewObject(L"Buffer", link_to));
  line_environment->Define(
      L"buffer_position",
      Value::NewObject(L"LineColumn", std::make_shared<LineColumn>(position)));
}

// Modifles line_options.contents, appending to it from input.
void AddContents(const OpenBuffer& source, const Line& input,
                 Line::Options* line_options) {
  auto trim = StringTrimLeft(
      input.contents(), source.Read(buffer_variables::line_prefix_characters));
  CHECK_LE(trim->size(), input.contents()->size());
  auto characters_trimmed =
      ColumnNumberDelta(input.contents()->size() - trim->size());
  auto initial_length = line_options->EndColumn().ToDelta();
  line_options->contents = StringAppend(line_options->contents, trim);
  for (auto& m : input.modifiers()) {
    if (m.first >= ColumnNumber(0) + characters_trimmed) {
      line_options->modifiers[m.first + initial_length - characters_trimmed] =
          m.second;
    }
  }
}

void AppendLine(const std::shared_ptr<OpenBuffer>& source,
                std::shared_ptr<LazyString> padding, LineColumn position,
                OpenBuffer* target) {
  Line::Options options;
  options.contents = padding;
  AddContents(*source, *source->LineAt(position.line), &options);
  target->AppendRawLine(std::make_shared<Line>(options));
  AdjustLastLine(target, source, position);
}

void DisplayTree(const std::shared_ptr<OpenBuffer>& source, size_t depth_left,
                 const ParseTree& tree, std::shared_ptr<LazyString> padding,
                 OpenBuffer* target) {
  for (size_t i = 0; i < tree.children().size(); i++) {
    auto& child = tree.children()[i];
    if (child.range().begin.line + LineNumberDelta(1) ==
            child.range().end.line ||
        depth_left == 0 || child.children().empty()) {
      Line::Options options;
      options.contents = padding;
      AddContents(*source, *source->LineAt(child.range().begin.line), &options);
      if (child.range().begin.line + LineNumberDelta(1) <
          child.range().end.line) {
        options.contents =
            StringAppend(options.contents, NewLazyString(L" ... "));
      } else {
        options.contents = StringAppend(options.contents, NewLazyString(L" "));
      }
      if (i + 1 >= tree.children().size() ||
          child.range().end.line != tree.children()[i + 1].range().begin.line) {
        AddContents(*source, *source->LineAt(child.range().end.line), &options);
      }
      target->AppendRawLine(std::make_shared<Line>(options));
      AdjustLastLine(target, source, child.range().begin);
      continue;
    }

    AppendLine(source, padding, child.range().begin, target);
    if (depth_left > 0) {
      DisplayTree(source, depth_left - 1, child,
                  StringAppend(NewLazyString(L"  "), padding), target);
    }
    if (i + 1 >= tree.children().size() ||
        child.range().end.line != tree.children()[i + 1].range().begin.line) {
      AppendLine(source, padding, child.range().end, target);
    }
  }
}

futures::Value<PossibleError> GenerateContents(
    EditorState* editor_state, std::weak_ptr<OpenBuffer> source_weak,
    OpenBuffer* target) {
  target->ClearContents(BufferContents::CursorsBehavior::kUnmodified);
  for (const auto& dir : editor_state->edge_path()) {
    target->EvaluateFile(PathJoin(dir, L"hooks/navigation-buffer-reload.cc"));
  }
  auto source = source_weak.lock();
  if (source == nullptr) {
    target->AppendToLastLine(NewLazyString(L"Source buffer no longer loaded."));
    return futures::Past(Success());
  }

  auto tree = source->simplified_parse_tree();
  if (tree == nullptr) {
    target->AppendToLastLine(NewLazyString(L"Target has no tree."));
    return futures::Past(Success());
  }

  target->AppendToLastLine(NewLazyString(source->Read(buffer_variables::name)));
  auto depth_value =
      target->environment()->Lookup(kDepthSymbol, VMType::Integer());
  int depth = depth_value == nullptr ? 3 : size_t(max(0, depth_value->integer));
  DisplayTree(source, depth, *tree, EmptyString(), target);
  return futures::Past(Success());
}

class NavigationBufferCommand : public Command {
 public:
  wstring Description() const override {
    return L"displays a navigation view of the current buffer";
  }
  wstring Category() const override { return L"Navigate"; }

  void ProcessInput(wint_t, EditorState* editor_state) override {
    auto source = editor_state->current_buffer();
    if (source == nullptr) {
      editor_state->status()->SetWarningText(
          L"NavigationBuffer needs an existing buffer.");
      return;
    }
    if (source->simplified_parse_tree() == nullptr) {
      source->status()->SetInformationText(L"Buffer has no tree.");
      return;
    }

    auto name = L"Navigation: " + source->Read(buffer_variables::name);
    auto it = editor_state->buffers()->insert(make_pair(name, nullptr));
    if (it.second) {
      OpenBuffer::Options options;
      options.editor = editor_state;
      options.name = name;
      std::weak_ptr<OpenBuffer> source_weak = source;
      options.generate_contents = [editor_state,
                                   source_weak](OpenBuffer* target) {
        return GenerateContents(editor_state, source_weak, target);
      };
      auto buffer = OpenBuffer::New(std::move(options));
      buffer->Set(buffer_variables::show_in_buffers_list, false);
      buffer->Set(buffer_variables::push_positions_to_history, false);
      buffer->Set(buffer_variables::allow_dirty_delete, true);
      buffer->environment()->Define(kDepthSymbol, Value::NewInteger(3));
      buffer->Set(buffer_variables::reload_on_enter, true);
      it.first->second = buffer;
      editor_state->StartHandlingInterrupts();
    }
    editor_state->set_current_buffer(it.first->second);
    editor_state->status()->Reset();
    it.first->second->Reload();
    editor_state->PushCurrentPosition();
    it.first->second->ResetMode();
    editor_state->ResetRepetitions();
  }
};
}  // namespace

std::unique_ptr<Command> NewNavigationBufferCommand() {
  return std::make_unique<NavigationBufferCommand>();
}

}  // namespace editor
}  // namespace afc
