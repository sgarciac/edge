#ifndef __AFC_EDITOR_EDITOR_H__
#define __AFC_EDITOR_EDITOR_H__

#include <ctime>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "src/args.h"
#include "src/audio.h"
#include "src/buffer.h"
#include "src/buffer_widget.h"
#include "src/buffers_list.h"
#include "src/command_mode.h"
#include "src/direction.h"
#include "src/editor_mode.h"
#include "src/editor_variables.h"
#include "src/lazy_string.h"
#include "src/line_marks.h"
#include "src/modifiers.h"
#include "src/status.h"
#include "src/transformation.h"
#include "src/transformation/type.h"
#include "src/widget.h"
#include "src/widget_list.h"
#include "src/work_queue.h"
#include "vm/public/environment.h"
#include "vm/public/vm.h"

namespace afc::editor {

using namespace afc::vm;

using std::list;
using std::map;
using std::max;
using std::min;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;

class EditorState {
 public:
  struct ScreenState {
    bool needs_hard_redraw = false;
  };

  EditorState(CommandLineValues args, AudioPlayer* audio_player);
  ~EditorState();

  const bool& Read(const EdgeVariable<bool>* variable) const;
  void Set(const EdgeVariable<bool>* variable, bool value);
  void toggle_bool_variable(const EdgeVariable<bool>* variable);

  void CheckPosition();

  void CloseBuffer(OpenBuffer* buffer);

  const map<wstring, shared_ptr<OpenBuffer>>* buffers() const {
    return &buffers_;
  }

  map<wstring, shared_ptr<OpenBuffer>>* buffers() { return &buffers_; }
  BuffersList* buffer_tree() { return &buffer_tree_; }

  void set_current_buffer(shared_ptr<OpenBuffer> buffer);
  void AddHorizontalSplit();
  void AddVerticalSplit();
  void SetHorizontalSplitsWithAllBuffers();
  void SetActiveBuffer(size_t position);
  void AdvanceActiveBuffer(int delta);
  void AdvanceActiveLeaf(int delta);
  void ZoomToLeaf();

  bool has_current_buffer() const;
  shared_ptr<OpenBuffer> current_buffer();
  const shared_ptr<OpenBuffer> current_buffer() const;
  // Returns the set of buffers that should be modified by commands.
  std::vector<std::shared_ptr<OpenBuffer>> active_buffers() const;
  void AddBuffer(std::shared_ptr<OpenBuffer> buffer,
                 BuffersList::AddBufferType insertion_type);
  futures::Value<EmptyValue> ForEachActiveBuffer(
      std::function<
          futures::Value<EmptyValue>(const std::shared_ptr<OpenBuffer>&)>
          callback);
  // Similar to ForEachActiveBuffer, but if repetions are set, only runs the
  // callback for the buffer referenced by repetitions (in the list of buffers,
  // buffer_tree_).
  futures::Value<EmptyValue> ForEachActiveBufferWithRepetitions(
      std::function<
          futures::Value<EmptyValue>(const std::shared_ptr<OpenBuffer>&)>
          callback);

  // Convenience wrapper of `ForEachActiveBuffer` that applies `transformation`.
  futures::Value<EmptyValue> ApplyToActiveBuffers(
      transformation::Variant transformation);

  wstring GetUnusedBufferName(const wstring& prefix);
  std::optional<int> exit_value() const { return exit_value_; }

  enum class TerminationType { kWhenClean, kIgnoringErrors };
  void Terminate(TerminationType termination_type, int exit_value);

  void ResetModifiers() {
    auto buffer = current_buffer();
    if (buffer != nullptr) {
      buffer->ResetMode();
    }
    modifiers_.ResetSoft();
  }

  Direction direction() const { return modifiers_.direction; }
  void set_direction(Direction direction) { modifiers_.direction = direction; }
  void ResetDirection() { modifiers_.ResetDirection(); }
  Direction default_direction() const { return modifiers_.default_direction; }
  void set_default_direction(Direction direction) {
    modifiers_.default_direction = direction;
    ResetDirection();
  }

  std::optional<size_t> repetitions() const { return modifiers_.repetitions; }
  void ResetRepetitions() { modifiers_.ResetRepetitions(); }
  void set_repetitions(size_t value) { modifiers_.repetitions = value; }

  // TODO: Maybe use a compiled regexp?
  const wstring& last_search_query() const { return last_search_query_; }
  void set_last_search_query(const wstring& query) {
    last_search_query_ = query;
  }

  Modifiers modifiers() const { return modifiers_; }
  void set_modifiers(const Modifiers& modifiers) { modifiers_ = modifiers; }

  Structure* structure() const { return modifiers_.structure; }
  void set_structure(Structure* structure) { modifiers_.structure = structure; }
  void ResetStructure() { modifiers_.ResetStructure(); }

  bool sticky_structure() const { return modifiers_.sticky_structure; }
  void set_sticky_structure(bool sticky_structure) {
    modifiers_.sticky_structure = sticky_structure;
  }

  Modifiers::ModifyMode insertion_modifier() const {
    return modifiers_.insertion;
  }
  void set_insertion_modifier(Modifiers::ModifyMode insertion_modifier) {
    modifiers_.insertion = insertion_modifier;
  }
  void ResetInsertionModifier() { modifiers_.ResetInsertion(); }
  Modifiers::ModifyMode default_insertion_modifier() const {
    return modifiers_.default_insertion;
  }
  void set_default_insertion_modifier(
      Modifiers::ModifyMode default_insertion_modifier) {
    modifiers_.default_insertion = default_insertion_modifier;
  }

  void ProcessInputString(const string& input) {
    for (size_t i = 0; i < input.size(); ++i) {
      ProcessInput(input[i]);
    }
  }

  void ProcessInput(int c);

  const LineMarks* line_marks() const { return &line_marks_; }
  LineMarks* line_marks() { return &line_marks_; }

  std::shared_ptr<MapModeCommands> default_commands() const {
    return default_commands_;
  }

  void MoveBufferForwards(size_t times);
  void MoveBufferBackwards(size_t times);

  ScreenState FlushScreenState();
  void set_screen_needs_hard_redraw(bool value) {
    std::unique_lock<std::mutex> lock(mutex_);
    screen_state_.needs_hard_redraw = value;
  }

  void PushCurrentPosition();
  void PushPosition(LineColumn position);
  std::shared_ptr<OpenBuffer> GetConsole();
  bool HasPositionsInStack();
  BufferPosition ReadPositionsStack();
  bool MovePositionsStack(Direction direction);

  Status* status();
  const Status* status() const;

  const wstring& home_directory() const { return home_directory_; }
  const vector<wstring>& edge_path() const { return edge_path_; }

  std::shared_ptr<Environment> environment() { return environment_; }

  wstring expand_path(const wstring& path) const;

  void PushSignal(int signal) { pending_signals_.push_back(signal); }
  void ProcessSignals();
  void StartHandlingInterrupts() { handling_interrupts_ = true; }
  bool handling_interrupts() const { return handling_interrupts_; }
  bool handling_stop_signals() const;

  int fd_to_detect_internal_events() const {
    return pipe_to_communicate_internal_events_.first;
  }

  void NotifyInternalEvent();

  AudioPlayer* audio_player() const { return audio_player_; }

  // Can return null.
  std::shared_ptr<EditorMode> keyboard_redirect() const {
    return keyboard_redirect_;
  }
  void set_keyboard_redirect(std::shared_ptr<EditorMode> keyboard_redirect) {
    keyboard_redirect_ = std::move(keyboard_redirect);
  }

  // Executes pending work from all buffers.
  void ExecutePendingWork();
  std::optional<struct timespec> WorkQueueNextExecution() const;
  WorkQueue* work_queue() const;

 private:
  std::shared_ptr<Environment> BuildEditorEnvironment();

  EdgeStructInstance<bool> bool_variables_;

  map<wstring, shared_ptr<OpenBuffer>> buffers_;
  std::optional<int> exit_value_;

  const wstring home_directory_;
  vector<wstring> edge_path_;

  const std::shared_ptr<Environment> environment_;

  wstring last_search_query_;

  // Should only be directly used when the editor has no buffer.
  std::shared_ptr<MapModeCommands> default_commands_;
  std::shared_ptr<EditorMode> keyboard_redirect_;

  std::mutex mutex_;
  ScreenState screen_state_;

  // Initially we don't consume SIGINT: we let it crash the process (in case
  // the user has accidentally ran Edge). However, as soon as the user starts
  // actually using Edge (e.g. modifies a buffer), we start consuming it.
  bool handling_interrupts_ = false;

  vector<int> pending_signals_;

  Modifiers modifiers_;
  LineMarks line_marks_;

  // Each editor has a pipe. The customer of the editor can read from the read
  // end, to detect the need to redraw the screen. Internally, background
  // threads write to the write end to trigger that.
  const std::pair<int, int> pipe_to_communicate_internal_events_;

  AudioPlayer* const audio_player_;

  BuffersList buffer_tree_;
  Status status_;
  mutable WorkQueue work_queue_;
};

}  // namespace afc::editor

#endif
