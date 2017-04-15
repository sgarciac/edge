#include "transformation_delete.h"

#include <glog/logging.h>

#include "buffer.h"
#include "direction.h"
#include "editor.h"
#include "lazy_string_append.h"
#include "modifiers.h"
#include "transformation.h"
#include "transformation_move.h"
#include "wstring.h"

namespace afc {
namespace editor {

namespace {
class DeleteCharactersTransformation : public Transformation {
 public:
  DeleteCharactersTransformation(DeleteOptions options) : options_(options) {}

  void Apply(
      EditorState* editor_state, OpenBuffer* buffer, Result* result) const {
    LOG(INFO) << "Starting DeleteCharactersTransformation: "
              << options_.modifiers << ", cursor: " << result->cursor;
    if (buffer->contents()->empty()) {
      result->success = false;
      return;
    }
    if (options_.modifiers.repetitions == 0) {
      VLOG(5) << "No repetitions.";
      return;
    }
    buffer->AdjustLineColumn(&result->cursor);
    if (options_.modifiers.direction == BACKWARDS) {
      for (size_t i = 0; i < options_.modifiers.repetitions; i++) {
        result->cursor = buffer->PositionBefore(result->cursor);
      }
    }

    size_t current_line = result->cursor.line;
    if (buffer->LineAt(current_line) == nullptr) {
      result->made_progress = false;
      return;
    }

    size_t chars_erased;
    size_t line_end = SkipLinesToErase(
        buffer, options_.modifiers.repetitions + result->cursor.column,
        result->cursor.line, &chars_erased);
    LOG(INFO) << "Erasing from line " << current_line << " to line " << line_end
              << " would erase " << chars_erased << " characters.";
    chars_erased -= result->cursor.column;

    // The amount of characters that should be erased from the current line. If
    // the line is the current line, this already includes characters in the
    // prefix.
    size_t chars_erase_line = buffer->LineAt(line_end)->size() + 1
        - min(buffer->LineAt(line_end)->size(),
              (options_.modifiers.repetitions < chars_erased
                   ? chars_erased - options_.modifiers.repetitions
                   : 0));
    if (chars_erase_line > buffer->LineAt(line_end)->size()) {
      LOG(INFO) << "Adjusting for end of buffer.";
      CHECK_EQ(chars_erase_line, buffer->LineAt(line_end)->size() + 1);
      chars_erase_line = 0;
      if (line_end + 1 >= buffer->lines_size()) {
        chars_erase_line = buffer->LineAt(line_end)->size();
      } else {
        line_end++;
      }
    }
    LOG(INFO) << "Characters to erase from current line: " << chars_erase_line
              << ", modifiers: " << options_.modifiers << ", chars_erased: "
              << chars_erased << ", actual length: "
              << buffer->LineAt(line_end)->size();

    result->success = chars_erased >= options_.modifiers.repetitions;
    result->made_progress = chars_erased + chars_erase_line > 0;

    shared_ptr<OpenBuffer> delete_buffer = GetDeletedTextBuffer(
        editor_state, buffer, current_line, line_end, result->cursor.column,
        chars_erase_line);
    if (options_.copy_to_paste_buffer) {
      VLOG(5) << "Preparing delete buffer.";
      result->delete_buffer->ApplyToCursors(
          TransformationAtPosition(
              result->delete_buffer->position(),
              NewInsertBufferTransformation(delete_buffer, 1, END)));
    }

    if (!options_.delete_region) {
      LOG(INFO) << "Not actually deleting region.";
      return;
    }

    LOG(INFO) << "Storing new line (at position " << line_end << ").";
    if (current_line == line_end) {
      buffer->DeleteCharactersFromLine(
          current_line, result->cursor.column,
          chars_erase_line - result->cursor.column);
    } else {
      buffer->DeleteUntilEnd(current_line, result->cursor.column);
      buffer->DeleteCharactersFromLine(line_end, 0, chars_erase_line);
      buffer->EraseLines(current_line + 1, line_end);  // Lines in the middle.
      buffer->FoldNextLine(current_line);
    }
    AdjustCursors(
        buffer, line_end, result->cursor.column, chars_erase_line);

    result->modified_buffer = true;

    result->undo_stack->PushFront(TransformationAtPosition(result->cursor,
        NewInsertBufferTransformation(
            delete_buffer, 1,
            options_.modifiers.direction == FORWARDS ? START : END)));
  }

  unique_ptr<Transformation> Clone() {
    return NewDeleteCharactersTransformation(options_);
  }

 private:
  // line: the position where the cursor was when the deletion started.
  // preserved_contents: the part of line that survives the deletion.
  // chars_erase_line: the number of characters erased from current line
  // (includes preserved_contents).
  void AdjustCursors(OpenBuffer* buffer, size_t line, size_t preserved_contents,
                     size_t chars_erase_line) const {
    buffer->AdjustCursors(
        [line, preserved_contents, chars_erase_line](LineColumn cursor) {
          if (cursor.line == line) {
            if (cursor.column > chars_erase_line) {
              cursor.column += preserved_contents - chars_erase_line;
            } else if (cursor.column > preserved_contents) {
              cursor.column = preserved_contents;
            }
          }
          return cursor;
        });
  }

  shared_ptr<OpenBuffer> GetDeletedTextBuffer(
      EditorState* editor_state, OpenBuffer* buffer, size_t line_begin,
      size_t line_end, size_t start, size_t chars_erase_line) const {
    LOG(INFO) << "Preparing deleted text buffer.";
    auto delete_buffer =
        std::make_shared<OpenBuffer>(editor_state, OpenBuffer::kPasteBuffer);
    auto first_line = std::make_shared<Line>(*buffer->LineAt(line_begin));
    if (line_begin == line_end) {
      first_line->DeleteCharacters(chars_erase_line);
    }
    first_line->DeleteCharacters(0, start);
    delete_buffer->AppendToLastLine(editor_state,
        first_line->contents(), first_line->modifiers());

    for (size_t i = line_begin + 1; i <= line_end; i++) {
      auto line = std::make_shared<Line>(*buffer->LineAt(i));
      if (i == line_end) {
        line->DeleteCharacters(chars_erase_line);
      }
      delete_buffer->AppendRawLine(editor_state, line);
    }

    return delete_buffer;
  }

  // Find and return the nearest (to line) line A such that if we erase all
  // characters in every line (including \n separators) between the current line
  // and A (including both), we will have erased at least as may characters as
  // chars_to_erase.
  //
  // chars_erased will be set to the total number of characters erased from the
  // current position until (including) line.
  size_t SkipLinesToErase(const OpenBuffer* buffer, size_t chars_to_erase,
                          size_t line, size_t* chars_erased) const {
    *chars_erased = 0;
    if (line == buffer->contents()->size()) {
      return line;
    }
    size_t newlines = 1;
    while (true) {
      CHECK_LT(line, buffer->contents()->size());
      LOG(INFO) << "Iteration at line " << line << " having already erased "
                << *chars_erased << " characters.";
      size_t chars_in_line = buffer->LineAt(line)->size() + newlines;
      LOG(INFO) << "Characters available in line: " << chars_in_line;
      *chars_erased += chars_in_line;
      if (*chars_erased >= chars_to_erase) {
        return line;
      }
      if (line + 1 >= buffer->lines_size()) {
        return line;
      }
      line++;
      newlines = 1;
    }
  }

  const DeleteOptions options_;
};

class DeleteRegionTransformation : public Transformation {
 public:
  DeleteRegionTransformation(DeleteOptions options) : options_(options) {}

  void Apply(
      EditorState* editor_state, OpenBuffer* buffer, Result* result) const {
    CHECK(buffer != nullptr);
    CHECK(result != nullptr);

    buffer->AdjustLineColumn(&result->cursor);
    const LineColumn adjusted_original_cursor = result->cursor;

    LineColumn start, end;
    if (!buffer->FindPartialRange(options_.modifiers, result->cursor, &start,
                                  &end)) {
      result->success = false;
      LOG(INFO) << "Unable to bind region, giving up.";
      return;
    }

    LOG(INFO) << "Starting at " << result->cursor << ", bound region at ["
              << start << ", " << end << ")";

    start = min(start, result->cursor);
    end = max(end, result->cursor);

    CHECK_LE(start, end);

    TransformationStack stack;

    stack.PushBack(NewGotoPositionTransformation(start));
    if (start.line < end.line) {
      LOG(INFO) << "Deleting superfluous lines (from " << start.line << " to "
                << end.line;
      while (start.line < end.line) {
        DeleteOptions delete_options;
        delete_options.delete_region = options_.delete_region;
        delete_options.modifiers.structure_range =
            Modifiers::FROM_CURRENT_POSITION_TO_END;
        stack.PushBack(
            TransformationAtPosition(start,
                NewDeleteLinesTransformation(delete_options)));
        if (options_.delete_region) {
          end.line--;
        } else {
          start.line++;
          start.column = 0;
        }
      }
      end.column += start.column;
    }

    CHECK_LE(start, end);
    CHECK_LE(start.column, end.column);
    DeleteOptions delete_options;
    delete_options.modifiers.repetitions = end.column - start.column;
    delete_options.delete_region = options_.delete_region;
    LOG(INFO) << "Deleting characters: " << options_.modifiers.repetitions;
    stack.PushBack(NewDeleteCharactersTransformation(delete_options));
    if (!options_.delete_region) {
      stack.PushBack(NewGotoPositionTransformation(adjusted_original_cursor));
    }
    stack.Apply(editor_state, buffer, result);
  }

  unique_ptr<Transformation> Clone() {
    return NewDeleteRegionTransformation(options_);
  }

 private:
  const DeleteOptions options_;
};

class DeleteLinesTransformation : public Transformation {
 public:
  DeleteLinesTransformation(DeleteOptions options) : options_(options) {}

  void Apply(
      EditorState* editor_state, OpenBuffer* buffer, Result* result) const {
    CHECK(buffer != nullptr);
    buffer->AdjustLineColumn(&result->cursor);
    const LineColumn adjusted_original_cursor = result->cursor;
    size_t repetitions = min(options_.modifiers.repetitions,
                             buffer->contents()->size() - result->cursor.line);
    shared_ptr<OpenBuffer> delete_buffer(
        new OpenBuffer(editor_state, OpenBuffer::kPasteBuffer));

    LOG(INFO) << "Erasing lines " << repetitions << " starting at line "
         << result->cursor.line << " in a buffer with size "
         << buffer->contents()->size() << " with modifiers: "
         << options_.modifiers;

    bool forwards = options_.modifiers.structure_range
        != Modifiers::FROM_BEGINNING_TO_CURRENT_POSITION;
    bool backwards = options_.modifiers.structure_range
        != Modifiers::FROM_CURRENT_POSITION_TO_END;

    bool deletes_ends_of_lines = options_.modifiers.strength > Modifiers::WEAK;
    TransformationStack stack;

    size_t line = result->cursor.line;
    for (size_t i = 0; i < repetitions; i++) {
      auto contents = buffer->LineAt(line + i);
      DVLOG(5) << "Erasing line: " << contents->ToString();
      size_t start = backwards ? 0 : result->cursor.column;
      size_t end = forwards ? contents->size() : result->cursor.column;
      if (start == 0 && end == contents->size() && options_.delete_region) {
        auto target_buffer = buffer->GetBufferFromCurrentLine();
        if (target_buffer.get() != buffer && target_buffer != nullptr) {
          auto it = editor_state->buffers()->find(target_buffer->name());
          if (it != editor_state->buffers()->end()) {
            editor_state->CloseBuffer(it);
          }
        }

        if (buffer->LineAt(result->cursor.line) != nullptr) {
          auto callback =
              buffer->LineAt(result->cursor.line)
                  ->environment()
                  ->Lookup(L"EdgeLineDeleteHandler");
          if (callback != nullptr
              && callback->type.type == vm::VMType::FUNCTION
              && callback->type.type_arguments.size() == 1
              && callback->type.type_arguments.at(0) == vm::VMType::VM_VOID) {
            LOG(INFO) << "Running EdgeLineDeleteHandler.";
            callback->callback({});
          }
        }
      }
      DeleteOptions delete_options;
      delete_options.delete_region = options_.delete_region;
      delete_options.modifiers.repetitions = end - start
          + (deletes_ends_of_lines && end == contents->size() ? 1 : 0);
      LineColumn position(line, start);
      if (!deletes_ends_of_lines || !options_.delete_region) {
        position.line += i;
      }
      DVLOG(6) << "Modifiers for line: " << delete_options.modifiers;
      DVLOG(6) << "Position for line: " << position;
      stack.PushBack(
          TransformationAtPosition(position,
               NewDeleteCharactersTransformation(delete_options)));
    }
    if (!options_.delete_region) {
      stack.PushBack(NewGotoPositionTransformation(adjusted_original_cursor));
    }
    stack.Apply(editor_state, buffer, result);
  }

  unique_ptr<Transformation> Clone() {
    return NewDeleteLinesTransformation(options_);
  }

 private:
  size_t FindStartOfLine(OpenBuffer* buffer, const Line* line) const {
    if (options_.modifiers.strength == Modifiers::VERY_WEAK) {
      return FindSoftStartOfLine(buffer, line);
    }
    return 0;
  }
  static size_t FindSoftStartOfLine(OpenBuffer* buffer, const Line* line) {
    const wstring& word_chars =
        buffer->read_string_variable(buffer->variable_word_characters());
    size_t start = 0;
    while (start < line->size()
           && word_chars.find(line->get(start)) == wstring::npos) {
      start++;
    }
    return start;
  }

  size_t FindLengthOfLine(OpenBuffer* buffer, const Line* line) const {
    if (options_.modifiers.strength == Modifiers::VERY_WEAK) {
      return FindSoftLengthOfLine(buffer, line);
    }
    return line->size();
  }

  static size_t FindSoftLengthOfLine(OpenBuffer* buffer, const Line* line) {
    const wstring& word_chars =
        buffer->read_string_variable(buffer->variable_word_characters());
    size_t length = line->size();
    while (length > 0
           && word_chars.find(line->get(length - 1)) == wstring::npos) {
      length--;
    }
    return length;
  }

  const DeleteOptions options_;
};

class DeleteBufferTransformation : public Transformation {
 public:
  DeleteBufferTransformation(DeleteOptions options) : options_(options) {}

  void Apply(
      EditorState* editor_state, OpenBuffer* buffer, Result* result) const {
    LOG(INFO) << "Erasing buffer (modifiers: " << options_.modifiers << ") of size: "
              << buffer->contents()->size();

    int current_line = result->cursor.line;
    int last_line = buffer->contents()->size();

    int begin = 0;
    int end = last_line;
    switch (options_.modifiers.structure_range) {
      case Modifiers::ENTIRE_STRUCTURE:
        break;  // We're all set.
      case Modifiers::FROM_BEGINNING_TO_CURRENT_POSITION:
        end = current_line;
        break;
      case Modifiers::FROM_CURRENT_POSITION_TO_END:
        begin = current_line;
        break;
    }

    CHECK_LE(begin, end);
    // TODO(alejo): Handle reverse?
    DeleteOptions options = options_;
    options.modifiers.repetitions = end - begin;
    TransformationAtPosition(LineColumn(begin),
        NewDeleteLinesTransformation(options))
            ->Apply(editor_state, buffer, result);
  }

  unique_ptr<Transformation> Clone() {
    return NewDeleteBufferTransformation(options_);
  }

 private:
  const DeleteOptions options_;
};

class DeleteTransformation : public Transformation {
 public:
  DeleteTransformation(DeleteOptions options) : options_(options) {}

  void Apply(
      EditorState* editor_state, OpenBuffer* buffer, Result* result) const {
    LOG(INFO) << "Start delete transformation at " << result->cursor << ": "
              << options_.modifiers;
    unique_ptr<Transformation> delegate = NewNoopTransformation();
    switch (options_.modifiers.structure) {
      case CHAR:
        delegate = NewDeleteCharactersTransformation(options_);
        break;
      case WORD:
      case CURSOR:
      case TREE:
        delegate = NewDeleteRegionTransformation(options_);
        break;
      case LINE:
        delegate = NewDeleteLinesTransformation(options_);
        break;
      case BUFFER:
        delegate = NewDeleteBufferTransformation(options_);
        break;
      case MARK:
      case PAGE:
      case SEARCH:
        LOG(INFO) << "DeleteTransformation can't handle structure: "
                  << options_.modifiers.structure;
        break;
    }
    return delegate->Apply(editor_state, buffer, result);
  }

  unique_ptr<Transformation> Clone() {
    return NewDeleteTransformation(options_);
  }

 private:
  const DeleteOptions options_;
};

}  // namespace

unique_ptr<Transformation> NewDeleteCharactersTransformation(
    DeleteOptions options) {
  return unique_ptr<Transformation>(
      new DeleteCharactersTransformation(options));
}

unique_ptr<Transformation> NewDeleteRegionTransformation(
    DeleteOptions options) {
  return unique_ptr<Transformation>(new DeleteRegionTransformation(options));
}

unique_ptr<Transformation> NewDeleteLinesTransformation(DeleteOptions options) {
  return unique_ptr<Transformation>(new DeleteLinesTransformation(options));
}

unique_ptr<Transformation> NewDeleteBufferTransformation(
    DeleteOptions options) {
  return unique_ptr<Transformation>(new DeleteBufferTransformation(options));
}

unique_ptr<Transformation> NewDeleteTransformation(DeleteOptions options) {
  return unique_ptr<Transformation>(new DeleteTransformation(options));
}

}  // namespace editor
}  // namespace afc
