AUTOMAKE_OPTIONS = subdir-objects

SUBDIRS = glog-0.4.0

bin_PROGRAMS = edge benchmark test fuzz_test test_fuzz_parser test_fuzz_compiler test_fuzz_classes

noinst_PROGRAMS = lemon

EXTRA_DIST = \
src/vm/internal/cpp.y

COMMON_SOURCES = \
src/async_processor.cc \
src/async_processor.h \
src/audio.cc \
src/audio.h \
src/args.cc \
src/args.h \
src/naive_bayes.cc \
src/naive_bayes.h \
src/buffer.cc \
src/buffer.h \
src/buffer_contents.cc \
src/buffer_contents.h \
src/buffer_contents_util.cc \
src/buffer_contents_util.h \
src/buffer_metadata_output_producer.cc \
src/buffer_metadata_output_producer.h \
src/buffer_output_producer.cc \
src/buffer_output_producer.h \
src/buffer_terminal.cc \
src/buffer_terminal.h \
src/widget_list.cc \
src/widget_list.h \
src/buffer_variables.cc \
src/buffer_variables.h \
src/buffer_widget.cc \
src/buffer_widget.h \
src/buffers_list.cc \
src/buffers_list.h \
src/command_argument_mode.h \
src/command_line.h \
src/cpp_command.cc \
src/cpp_parse_tree.cc \
src/cpp_parse_tree.h \
src/char_buffer.cc \
src/command_mode.cc \
src/command_mode.h \
src/command_with_modifiers.cc \
src/command_with_modifiers.h \
src/const_tree.cc \
src/const_tree.h \
src/cursors.cc \
src/cursors.h \
src/decaying_counter.cc \
src/decaying_counter.h \
src/delete_mode.cc \
src/delete_mode.h \
src/dirname.cc \
src/dirname.h \
src/direction.cc \
src/editor.cc \
src/editor.h \
src/editor_variables.cc \
src/editor_variables.h \
src/frame_output_producer.cc \
src/frame_output_producer.h \
src/file_link_mode.cc \
src/file_system_driver.cc \
src/file_system_driver.h \
src/file_descriptor_reader.cc \
src/file_descriptor_reader.h \
src/find_mode.cc \
src/function_traits.h \
src/futures/futures.cc \
src/futures/futures.h \
src/fuzz.h \
src/fuzz.cc \
src/fuzz_testable.h \
src/fuzz_testable.cc \
src/goto_command.cc \
src/hash.h \
src/help_command.cc \
src/horizontal_split_output_producer.cc \
src/horizontal_split_output_producer.h \
src/insert_mode.cc \
src/lazy_string.cc \
src/lazy_string_append.cc \
src/lazy_string_functional.h \
src/lazy_string_trim.cc \
src/lazy_string_trim.h \
src/line.cc \
src/line.h \
src/line_column.cc \
src/line_marks.cc \
src/line_modifier.cc \
src/line_modifier.h \
src/line_number_output_producer.h \
src/line_number_output_producer.cc \
src/line_prompt_mode.cc \
src/line_scroll_control.h \
src/line_scroll_control.cc \
src/list_buffers_command.cc \
src/log.cc \
src/log.h \
src/lowercase.cc \
src/lru_cache.h \
src/map_mode.cc \
src/modifiers.cc \
src/modifiers.h \
src/navigation_buffer.cc \
src/navigation_buffer.h \
src/navigate_command.cc \
src/notification.cc \
src/notification.h \
src/open_directory_command.cc \
src/open_file_command.cc \
src/output_producer.cc \
src/output_producer.h \
src/parse_tools.cc \
src/parse_tools.h \
src/parse_tree.cc \
src/parse_tree.h \
src/parsers/diff.cc \
src/parsers/diff.h \
src/parsers/markdown.cc \
src/parsers/markdown.h \
src/predictor.cc \
src/quit_command.cc \
src/record_command.cc \
src/repeat_mode.cc \
src/run_command_handler.cc \
src/run_cpp_command.cc \
src/run_cpp_command.h \
src/run_cpp_file.cc \
src/run_cpp_file.h \
src/screen.h \
src/screen_curses.cc \
src/screen_curses.h \
src/screen_vm.cc \
src/screen_vm.h \
src/search_command.cc \
src/search_handler.cc \
src/section_brackets_producer.cc \
src/section_brackets_producer.h \
src/seek.cc \
src/seek.h \
src/send_end_of_file_command.cc \
src/server.cc \
src/set_buffer_mode.cc \
src/set_buffer_mode.h \
src/set_mode_command.cc \
src/set_mode_command.h \
src/set_variable_command.cc \
src/set_variable_command.h \
src/shapes.cc \
src/shapes.h \
src/status.cc \
src/status.h \
src/status_output_producer.cc \
src/status_output_producer.h \
src/structure.cc \
src/substring.cc \
src/terminal.cc \
src/tests/benchmarks.cc \
src/tests/benchmarks.h \
src/tests/tests.cc \
src/tests/tests.h \
src/time.cc \
src/time.h \
src/tokenize.cc \
src/tokenize.h \
src/tokenize_test.cc \
src/tracker.cc \
src/tracker.h \
src/transformation/composite.cc \
src/transformation/composite.h \
src/transformation/cursors.cc \
src/transformation/cursors.h \
src/transformation/delete.cc \
src/transformation/delete.h \
src/transformation/expand.cc \
src/transformation/expand.h \
src/transformation/insert.cc \
src/transformation/insert.h \
src/transformation/move.cc \
src/transformation/move.h \
src/transformation/noop.cc \
src/transformation/noop.h \
src/transformation/repetitions.cc \
src/transformation/repetitions.h \
src/transformation/set_position.cc \
src/transformation/set_position.h \
src/transformation/stack.cc \
src/transformation/stack.h \
src/transformation/switch_case.cc \
src/transformation/switch_case.h \
src/transformation/tree_navigate.cc \
src/transformation/tree_navigate.h \
src/transformation/type.cc \
src/transformation/type.h \
src/transformation.cc \
src/transformation.h \
src/value_or_error.cc \
src/value_or_error.h \
src/vertical_split_output_producer.cc \
src/vertical_split_output_producer.h \
src/viewers.cc \
src/viewers.h \
src/vm_transformation.cc \
src/vm_transformation.h \
src/vm/public/optional.h \
src/vm/public/set.h \
src/vm/public/vector.h \
src/vm/internal/callbacks.cc \
src/vm/internal/lambda.cc \
src/vm/internal/lambda.h \
src/vm/internal/vm.cc \
src/vm/internal/value.cc \
src/vm/internal/string.cc \
src/vm/internal/types.cc \
src/vm/internal/binary_operator.cc \
src/vm/internal/if_expression.cc \
src/vm/internal/return_expression.cc \
src/vm/internal/while_expression.cc \
src/vm/internal/constant_expression.cc \
src/vm/internal/function_call.cc \
src/vm/internal/environment.cc \
src/vm/internal/assign_expression.cc \
src/vm/internal/append_expression.cc \
src/vm/internal/negate_expression.cc \
src/vm/internal/logical_expression.cc \
src/vm/internal/variable_lookup.cc \
src/vm/internal/wstring.cc \
src/widget.cc \
src/widget.h \
src/work_queue.cc \
src/work_queue.h \
src/wstring.cc

BENCHMARK_SOURCES = \
src/const_tree.cc \
src/const_tree.h \
src/tests/benchmarks.cc \
src/tests/benchmarks.h \
src/tests/tests.cc \
src/tests/tests.h \
src/time.cc \
src/time.h \
src/wstring.cc \
src/wstring.h

CLEANFILES = src/vm/internal/cpp.cc src/vm/internal/cpp.h

BUILT_SOURCES = src/vm/internal/cpp.cc src/vm/internal/cpp.h

docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)
dist_doc_DATA = README.md

edge_SOURCES = $(COMMON_SOURCES) src/main.cc

benchmark_SOURCES = $(BENCHMARK_SOURCES) src/tests/benchmarks_main.cc

test_SOURCES = \
$(COMMON_SOURCES) \
src/test/buffer_contents_test.cc \
src/test/buffer_contents_test.h \
src/test/line_test.cc \
src/test/line_test.h \
src/test.cc

fuzz_test_SOURCES = $(COMMON_SOURCES) src/fuzz_test.cc

test_fuzz_parser_SOURCES = $(COMMON_SOURCES) src/test/fuzz/parser.cc

test_fuzz_compiler_SOURCES = $(COMMON_SOURCES) src/test/fuzz/compiler.cc

test_fuzz_classes_SOURCES = $(COMMON_SOURCES) src/test/fuzz/classes.cc

lemon_SOURCES = src/vm/internal/lemon.c

src/vm/internal/cpp.h: src/vm/internal/cpp.y lemon
	./lemon -Tsrc/vm/internal/lempar.c src/vm/internal/cpp.y

src/vm/internal/cpp.cc: src/vm/internal/cpp.h

GLOG=glog-0.4.0/libglog_la-demangle.o \
glog-0.4.0/libglog_la-logging.o \
glog-0.4.0/libglog_la-symbolize.o \
glog-0.4.0/libglog_la-raw_logging.o \
glog-0.4.0/libglog_la-utilities.o \
glog-0.4.0/libglog_la-vlog_is_on.o

AM_CPPFLAGS=-std=c++17 -g -Wall -Wextra -Iglog-0.4.0/src -DDOCDIR='"$(docdir)"'
AM_LDFLAGS=-std=c++17 -g
edge_LDADD=glog-0.4.0/libglog.la
benchmark_LDADD=glog-0.4.0/libglog.la
test_LDADD=glog-0.4.0/libglog.la
fuzz_test_LDADD=glog-0.4.0/libglog.la
test_fuzz_parser_LDADD=glog-0.4.0/libglog.la
test_fuzz_compiler_LDADD=glog-0.4.0/libglog.la
test_fuzz_classes_LDADD=glog-0.4.0/libglog.la
