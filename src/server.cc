#include "src/server.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

extern "C" {
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
}

#include "src/buffer.h"
#include "src/buffer_variables.h"
#include "src/editor.h"
#include "src/file_link_mode.h"
#include "src/file_system_driver.h"
#include "src/lazy_string.h"
#include "src/vm/public/vm.h"
#include "src/wstring.h"

namespace afc {
namespace editor {

using namespace afc::vm;

using std::cerr;
using std::pair;
using std::shared_ptr;
using std::string;

struct Environment;

wstring CppEscapeString(wstring input) {
  wstring output;
  output.reserve(input.size() * 2);
  for (wchar_t c : input) {
    switch (c) {
      case '\n':
        output += L"\\n";
        break;
      case '"':
        output += L"\\\"";
        break;
      case '\\':
        output += L"\\\\";
        break;
      default:
        output += c;
    }
  }
  return output;
}

bool CreateFifo(wstring input_path, wstring* output, wstring* error) {
  while (true) {
    // Using mktemp here is secure: if the attacker creates the file, mkfifo
    // will fail.
    char* path_str = input_path.empty()
                         ? mktemp(strdup("/tmp/edge-server-XXXXXX"))
                         : strdup(ToByteString(input_path).c_str());
    if (mkfifo(path_str, 0600) == -1) {
      *error =
          FromByteString(path_str) + L": " + FromByteString(strerror(errno));
      free(path_str);
      if (!input_path.empty()) {
        return false;
      }
      continue;
    }
    *output = FromByteString(path_str);
    free(path_str);
    return true;
  }
}

int MaybeConnectToParentServer(wstring* error) {
  wstring dummy;
  if (error == nullptr) {
    error = &dummy;
  }

  const char* variable = "EDGE_PARENT_ADDRESS";
  char* server_address = getenv(variable);
  if (server_address == nullptr) {
    *error =
        L"Unable to find remote address (through environment variable "
        L"EDGE_PARENT_ADDRESS).";
    return -1;
  }
  return MaybeConnectToServer(string(server_address), error);
}

int MaybeConnectToServer(const string& address, wstring* error) {
  LOG(INFO) << "Connecting to server: " << address;
  wstring dummy;
  if (error == nullptr) {
    error = &dummy;
  }

  int fd = open(address.c_str(), O_WRONLY);
  if (fd == -1) {
    *error = FromByteString(address) +
             L": Connecting to server: open failed: " +
             FromByteString(strerror(errno));
    return -1;
  }
  wstring private_fifo;
  if (!CreateFifo(L"", &private_fifo, error)) {
    *error = L"Unable to create fifo for communication with server: " + *error;
    return -1;
  }
  LOG(INFO) << "Fifo created: " << private_fifo;
  string command = "ConnectTo(\"" + ToByteString(private_fifo) + "\");\n";
  LOG(INFO) << "Sending connection command: " << command;
  if (write(fd, command.c_str(), command.size()) == -1) {
    *error = FromByteString(address) + L": write failed: " +
             FromByteString(strerror(errno));
    return -1;
  }
  close(fd);

  LOG(INFO) << "Opening private fifo: " << private_fifo;
  int private_fd = open(ToByteString(private_fifo).c_str(), O_RDWR);
  LOG(INFO) << "Connection fd: " << private_fd;
  if (private_fd == -1) {
    *error =
        private_fifo + L": open failed: " + FromByteString(strerror(errno));
    return -1;
  }
  CHECK_GT(private_fd, -1);
  return private_fd;
}

void Daemonize(const std::unordered_set<int>& surviving_fds) {
  pid_t pid;

  pid = fork();
  CHECK_GE(pid, 0) << "fork failed: " << strerror(errno);
  if (pid > 0) {
    LOG(INFO) << "Parent exits.";
    exit(0);
  }

  CHECK_GT(setsid(), 0);
  signal(SIGHUP, SIG_IGN);

  pid = fork();
  CHECK_GE(pid, 0) << "fork failed: " << strerror(errno);
  if (pid > 0) {
    LOG(INFO) << "Parent exits.";
    exit(0);
  }

  for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
    if (surviving_fds.find(fd) == surviving_fds.end()) {
      close(fd);
    }
  }
}

futures::Value<PossibleError> GenerateContents(
    std::shared_ptr<FileSystemDriver> file_system_driver, OpenBuffer* target) {
  wstring address = target->Read(buffer_variables::path);
  LOG(INFO) << L"Server starts: " << address;
  return futures::Transform(
      OnError(file_system_driver->Open(address, O_RDONLY | O_NDELAY, 0),
              [address](Error error) {
                LOG(FATAL) << address
                           << ": Server: GenerateContents: Open failed: "
                           << error.description;
                return error;
              }),
      [target, address](int fd) {
        LOG(INFO) << "Server received connection: " << fd;
        target->SetInputFiles(fd, -1, false, -1);
        return Success();
      });
}

bool StartServer(EditorState* editor_state, wstring address,
                 wstring* actual_address, wstring* error) {
  wstring dummy;
  if (actual_address == nullptr) {
    actual_address = &dummy;
  }

  if (!CreateFifo(address, actual_address, error)) {
    *error = L"Error creating fifo: " + *error;
    return false;
  }

  LOG(INFO) << "Starting server: " << *actual_address;
  setenv("EDGE_PARENT_ADDRESS", ToByteString(*actual_address).c_str(), 1);
  auto buffer = OpenServerBuffer(editor_state, *actual_address);
  buffer->Set(buffer_variables::reload_after_exit, true);
  buffer->Set(buffer_variables::default_reload_after_exit, true);

  return true;
}

shared_ptr<OpenBuffer> OpenServerBuffer(EditorState* editor_state,
                                        const wstring& address) {
  OpenBuffer::Options options;
  options.editor = editor_state;
  options.name = editor_state->GetUnusedBufferName(L"- server");
  options.path = address;
  options.generate_contents =
      [file_system_driver = std::make_shared<FileSystemDriver>(
           editor_state->work_queue())](OpenBuffer* target) {
        return GenerateContents(file_system_driver, target);
      };

  auto buffer = OpenBuffer::New(std::move(options));
  buffer->Set(buffer_variables::clear_on_reload, false);
  buffer->Set(buffer_variables::vm_exec, true);
  buffer->Set(buffer_variables::show_in_buffers_list, false);
  buffer->Set(buffer_variables::allow_dirty_delete, true);
  buffer->Set(buffer_variables::display_progress, false);

  editor_state->buffers()->insert(
      {buffer->Read(buffer_variables::name), buffer});
  buffer->Reload();
  return buffer;
}

}  // namespace editor
}  // namespace afc
