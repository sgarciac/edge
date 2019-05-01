#include "src/buffer_contents.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_set>

#include "src/char_buffer.h"
#include "src/test/line_test.h"
#include "src/wstring.h"

namespace afc {
namespace editor {
namespace testing {
namespace {
void TestBufferContentsSnapshot() {
  BufferContents contents;
  for (auto& s : {L"alejandro", L"forero", L"cuervo"}) {
    contents.push_back(std::make_shared<Line>(Line::Options(NewLazyString(s))));
  }
  auto copy = contents.copy();
  CHECK_EQ("\nalejandro\nforero\ncuervo", ToByteString(contents.ToString()));
  CHECK_EQ("\nalejandro\nforero\ncuervo", ToByteString(copy->ToString()));

  contents.SplitLine(LineColumn(2, ColumnNumber(3)));
  CHECK_EQ("\nalejandro\nfor\nero\ncuervo", ToByteString(contents.ToString()));
  CHECK_EQ("\nalejandro\nforero\ncuervo", ToByteString(copy->ToString()));
}

void TestBufferInsertModifiers() {
  BufferContents contents;
  Line::Options options;
  options.contents = NewLazyString(L"alejo");
  options.modifiers.assign(5, {LineModifier::CYAN});

  contents.push_back(std::make_shared<Line>(options));
  contents.push_back(std::make_shared<Line>(options));
  options.modifiers[2].insert(LineModifier::BOLD);
  contents.push_back(std::make_shared<Line>(options));
  auto line = std::make_shared<Line>(*contents.at(1));
  line->SetAllModifiers(LineModifierSet({LineModifier::DIM}));
  contents.push_back(line);

  for (int i = 0; i < 2; i++) {
    LOG(INFO) << "Start iteration: " << i;
    CHECK_EQ(contents.size(), 5);

    CHECK(contents.at(1)->modifiers()[0] ==
          LineModifierSet({LineModifier::CYAN}));
    CHECK(contents.at(1)->modifiers()[1] ==
          LineModifierSet({LineModifier::CYAN}));
    CHECK(contents.at(1)->modifiers()[2] ==
          LineModifierSet({LineModifier::CYAN}));

    CHECK(contents.at(2)->modifiers()[0] ==
          LineModifierSet({LineModifier::CYAN}));
    CHECK(contents.at(2)->modifiers()[2] ==
          LineModifierSet({LineModifier::CYAN}));

    CHECK(contents.at(3)->modifiers()[0] ==
          LineModifierSet({LineModifier::CYAN}));
    CHECK(contents.at(3)->modifiers()[2] ==
          LineModifierSet({LineModifier::CYAN, LineModifier::BOLD}));

    CHECK(contents.at(4)->modifiers()[0] ==
          LineModifierSet({LineModifier::DIM}));
    CHECK(contents.at(4)->modifiers()[2] ==
          LineModifierSet({LineModifier::DIM}));

    contents.SplitLine(LineColumn(1, ColumnNumber(2)));
    CHECK_EQ(contents.size(), 6);
    contents.FoldNextLine(1);
    CHECK_EQ(contents.size(), 5);

    contents.SplitLine(LineColumn(4, ColumnNumber(2)));
    CHECK_EQ(contents.size(), 6);
    contents.FoldNextLine(4);
    CHECK_EQ(contents.size(), 5);
  }
}

void TestCursorsMove() {
  BufferContents contents;
  std::vector<CursorsTracker::Transformation> transformations;
  contents.set_line(0, std::make_shared<Line>(L"aleandro forero cuervo"));
  contents.AddUpdateListener([&](const CursorsTracker::Transformation& t) {
    transformations.push_back(t);
  });
  CHECK_EQ(transformations.size(), 0);
  contents.InsertCharacter(0, ColumnNumber(3));
  CHECK_EQ(transformations.size(), 1);
  CHECK_EQ(transformations[0], CursorsTracker::Transformation());
  transformations.clear();

  contents.SetCharacter(0, ColumnNumber(3), 'j', {});
  CHECK_EQ(transformations.size(), 1);
  transformations.clear();
}
}  // namespace

void BufferContentsTests() {
  LOG(INFO) << "BufferContents tests: start.";
  TestBufferContentsSnapshot();
  TestBufferInsertModifiers();
  TestCursorsMove();
  LOG(INFO) << "BufferContents tests: done.";
}

}  // namespace testing
}  // namespace editor
}  // namespace afc
