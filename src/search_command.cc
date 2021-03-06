#include "src/search_command.h"

#include "src/buffer.h"
#include "src/buffer_variables.h"
#include "src/command.h"
#include "src/editor.h"
#include "src/line_prompt_mode.h"
#include "src/search_handler.h"
#include "src/transformation.h"

namespace afc::editor {
namespace {
using futures::IterationControlCommand;

static void MergeInto(AsyncSearchProcessor::Output current_results,
                      AsyncSearchProcessor::Output* final_results) {
  if (current_results.pattern_error.has_value()) {
  }
  final_results->matches += current_results.matches;
  switch (current_results.search_completion) {
    case AsyncSearchProcessor::Output::SearchCompletion::kInvalidPattern:
      final_results->pattern_error = current_results.pattern_error;
      final_results->search_completion =
          AsyncSearchProcessor::Output::SearchCompletion::kInvalidPattern;
      break;
    case AsyncSearchProcessor::Output::SearchCompletion::kInterrupted:
      final_results->search_completion =
          AsyncSearchProcessor::Output::SearchCompletion::kInterrupted;
      break;
    case AsyncSearchProcessor::Output::SearchCompletion::kFull:
      break;
  }
}

static void DoSearch(OpenBuffer* buffer, SearchOptions options) {
  buffer->set_active_cursors(SearchHandler(buffer->editor(), options, buffer));
  buffer->ResetMode();
}

ColorizePromptOptions SearchResultsModifiers(
    std::shared_ptr<LazyString> line, AsyncSearchProcessor::Output result) {
  using SC = AsyncSearchProcessor::Output::SearchCompletion;
  LineModifierSet modifiers;
  switch (result.search_completion) {
    case SC::kInvalidPattern:
      modifiers.insert(LineModifier::RED);
      CHECK(result.pattern_error.has_value());
      break;
    case SC::kInterrupted:
    case SC::kFull:
      switch (result.matches) {
        case 0:
          break;
        case 1:
          modifiers.insert(LineModifier::CYAN);
          break;
        case 2:
          modifiers.insert(LineModifier::YELLOW);
          break;
        default:
          modifiers.insert(LineModifier::GREEN);
          break;
      }
      break;
  }

  return {.tokens = {{{.value = L"",
                       .begin = ColumnNumber(0),
                       .end = ColumnNumber(0) + line->size()},
                      .modifiers = std::move(modifiers)}}};
}

// Wraps a progress channel and provides a builder to create "child" progress
// channels. Information added to the children gets aggregated before being
// propagated to the parent.
//
// This class isn't thread-safe.
class ProgressAggregator {
 public:
  ProgressAggregator(std::unique_ptr<ProgressChannel> parent_channel)
      : data_(std::make_shared<Data>(std::move(parent_channel))) {}

  std::unique_ptr<ProgressChannel> NewChild() {
    auto child_information = std::make_shared<ProgressInformation>();
    data_->children_created++;
    return std::make_unique<ProgressChannel>(
        data_->parent_channel->work_queue(),
        [data = data_, child_information](ProgressInformation information) {
          if (HasMatches(information) && !HasMatches(*child_information)) {
            data->buffers_with_matches++;
          }

          for (auto& [token, value] : information.counters) {
            auto& child_token_value = child_information->counters[token];
            data->aggregates.counters[token] -= child_token_value;
            child_token_value = value;
            data->aggregates.counters[token] += child_token_value;
          }

          for (auto& [token, value] : information.values) {
            data->aggregates.values[token] = value;
          }

          if (data->children_created > 1) {
            data->aggregates.values[L"buffers"] =
                std::to_wstring(data->buffers_with_matches) + L"/" +
                std::to_wstring(data->children_created);
          }

          data->parent_channel->Push(data->aggregates);
        },
        data_->parent_channel->consume_mode());
  }

 private:
  static bool HasMatches(const ProgressInformation& info) {
    auto it = info.counters.find(L"matches");
    return it != info.counters.end() && it->second > 0;
  }

  struct Data {
    Data(std::unique_ptr<ProgressChannel> parent_channel)
        : parent_channel(std::move(parent_channel)) {}

    const std::unique_ptr<ProgressChannel> parent_channel;

    ProgressInformation aggregates;
    size_t buffers_with_matches = 0;
    size_t children_created = 0;
  };
  const std::shared_ptr<Data> data_;
};

class SearchCommand : public Command {
 public:
  wstring Description() const override { return L"Searches for a string."; }
  wstring Category() const override { return L"Navigate"; }

  void ProcessInput(wint_t, EditorState* editor_state) {
    if (editor_state->structure()->search_query() ==
        Structure::SearchQuery::kRegion) {
      futures::Transform(
          editor_state->ForEachActiveBuffer(
              [editor_state](const std::shared_ptr<OpenBuffer>& buffer) {
                SearchOptions search_options;
                Range range = buffer->FindPartialRange(
                    editor_state->modifiers(), buffer->position());
                if (range.begin == range.end) {
                  return futures::Past(EmptyValue());
                }
                VLOG(5) << "FindPartialRange: [position:" << buffer->position()
                        << "][range:" << range
                        << "][modifiers:" << editor_state->modifiers() << "]";
                CHECK_LT(range.begin, range.end);
                if (range.end.line > range.begin.line) {
                  // This can happen when repetitions are used (to find multiple
                  // words). We just cap it at the start/end of the line.
                  if (editor_state->direction() == Direction::kBackwards) {
                    range.begin = LineColumn(range.end.line);
                  } else {
                    range.end = LineColumn(
                        range.begin.line,
                        buffer->LineAt(range.begin.line)->EndColumn());
                  }
                }
                CHECK_EQ(range.begin.line, range.end.line);
                if (range.begin == range.end) {
                  return futures::Past(EmptyValue());
                }
                CHECK_LT(range.begin.column, range.end.column);
                buffer->set_position(range.begin);
                search_options.search_query =
                    buffer->LineAt(range.begin.line)
                        ->Substring(range.begin.column,
                                    range.end.column - range.begin.column)
                        ->ToString();
                search_options.starting_position = buffer->position();
                DoSearch(buffer.get(), search_options);
                return futures::Past(EmptyValue());
              }),
          [editor_state](EmptyValue) {
            editor_state->ResetStructure();
            editor_state->ResetDirection();
            return futures::Past(EmptyValue());
          });
      return;
    }

    PromptOptions options;
    options.editor_state = editor_state;
    options.prompt = L"🔎 ";
    options.history_file = L"search";
    options.handler = [](const wstring& input, EditorState* editor_state) {
      return futures::Transform(
          editor_state->ForEachActiveBuffer(
              [editor_state, input](const std::shared_ptr<OpenBuffer>& buffer) {
                auto search_options = BuildPromptSearchOptions(
                    input, buffer.get(), std::make_shared<Notification>());
                if (search_options.has_value()) {
                  DoSearch(buffer.get(), search_options.value());
                }
                return futures::Past(EmptyValue());
              }),
          [editor_state](EmptyValue) {
            editor_state->ResetDirection();
            editor_state->ResetStructure();
            return futures::Past(EmptyValue());
          });
    };
    auto async_search_processor =
        std::make_shared<AsyncSearchProcessor>(editor_state->work_queue());

    options.colorize_options_provider =
        [editor_state, async_search_processor,
         buffers = std::make_shared<std::vector<std::shared_ptr<OpenBuffer>>>(
             editor_state->active_buffers())](
            const std::shared_ptr<LazyString>& line,
            std::unique_ptr<ProgressChannel> progress_channel,
            std::shared_ptr<Notification> abort_notification) {
          VLOG(5) << "Triggering async search.";
          auto results = std::make_shared<AsyncSearchProcessor::Output>();
          auto progress_aggregator =
              std::make_shared<ProgressAggregator>(std::move(progress_channel));
          return futures::Transform(
              futures::ForEach(
                  buffers->begin(), buffers->end(),
                  [editor_state, async_search_processor, line,
                   progress_aggregator, abort_notification,
                   results](const std::shared_ptr<OpenBuffer>& buffer) {
                    auto progress_channel = progress_aggregator->NewChild();
                    if (buffer->Read(buffer_variables::search_case_sensitive)) {
                      progress_channel->Push({.values = {{L"case", L"on"}}});
                    }
                    if (line->size().IsZero()) {
                      return futures::Past(
                          futures::IterationControlCommand::kContinue);
                    }
                    auto search_options = BuildPromptSearchOptions(
                        line->ToString(), buffer.get(), abort_notification);
                    if (!search_options.has_value()) {
                      VLOG(6) << "search_options has no value.";
                      return futures::Past(
                          futures::IterationControlCommand::kContinue);
                    }
                    VLOG(5) << "Starting search in buffer: "
                            << buffer->Read(buffer_variables::name);
                    return futures::Transform(
                        async_search_processor->Search(
                            search_options.value(), *buffer,
                            std::move(progress_channel)),
                        [results, abort_notification,
                         line](AsyncSearchProcessor::Output current_results) {
                          MergeInto(current_results, results.get());
                          return abort_notification->HasBeenNotified()
                                     ? futures::IterationControlCommand::kStop
                                     : futures::IterationControlCommand::
                                           kContinue;
                        });
                  }),
              [results, buffers, abort_notification,
               line](futures::IterationControlCommand iteration_result) {
                switch (iteration_result) {
                  case futures::IterationControlCommand::kStop:
                    return ColorizePromptOptions();
                  case futures::IterationControlCommand::kContinue:
                    VLOG(5) << "Drawing of search results.";
                    return SearchResultsModifiers(line, std::move(*results));
                  default:
                    LOG(FATAL) << "Invalid value for iteration_result.";
                    return ColorizePromptOptions();
                }
              });
        };

    options.predictor = SearchHandlerPredictor;
    options.status = PromptOptions::Status::kBuffer;
    Prompt(std::move(options));
  }

 private:
  static std::optional<SearchOptions> BuildPromptSearchOptions(
      std::wstring input, OpenBuffer* buffer,
      std::shared_ptr<Notification> abort_notification) {
    auto editor = buffer->editor();
    SearchOptions search_options;
    search_options.search_query = input;
    if (editor->structure()->search_range() ==
        Structure::SearchRange::kBuffer) {
      search_options.starting_position = buffer->position();
    } else {
      Range range =
          buffer->FindPartialRange(editor->modifiers(), buffer->position());
      if (range.begin == range.end) {
        buffer->status()->SetInformationText(L"Unable to extract region.");
        return std::nullopt;
      }
      CHECK_LE(range.begin, range.end);
      if (editor->modifiers().direction == Direction::kBackwards) {
        search_options.starting_position = range.end;
        search_options.limit_position = range.begin;
      } else {
        search_options.starting_position = range.begin;
        search_options.limit_position = range.end;
      }
      LOG(INFO) << "Searching region: " << search_options.starting_position
                << " to " << search_options.limit_position.value();
    }
    search_options.abort_notification = abort_notification;
    return search_options;
  }
};
}  // namespace

std::unique_ptr<Command> NewSearchCommand() {
  return std::make_unique<SearchCommand>();
}

}  // namespace afc::editor
