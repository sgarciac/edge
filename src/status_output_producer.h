#ifndef __AFC_EDITOR_STATUS_OUTPUT_PRODUCER_H__
#define __AFC_EDITOR_STATUS_OUTPUT_PRODUCER_H__

#include <cwchar>
#include <list>
#include <memory>
#include <string>

#include "src/modifiers.h"
#include "src/output_producer.h"

namespace afc {
namespace editor {

class Status;
class OpenBuffer;

class StatusOutputProducer : public OutputProducer {
 public:
  // `buffer` may be null.
  StatusOutputProducer(const Status* status, const OpenBuffer* buffer,
                       Modifiers modifiers);

  Generator Next();

 private:
  const Status* const status_;
  const OpenBuffer* const buffer_;
  const Modifiers modifiers_;
};

}  // namespace editor
}  // namespace afc

#endif  // __AFC_EDITOR_STATUS_OUTPUT_PRODUCER_H__