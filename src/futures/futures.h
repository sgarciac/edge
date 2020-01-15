#include <glog/logging.h>

#include <functional>
#include <type_traits>

#ifndef __AFC_EDITOR_FUTURES_FUTURES_H__
#define __AFC_EDITOR_FUTURES_FUTURES_H__

namespace afc::futures {

template <typename Type>
class Future;

template <typename Type>
class Value {
 public:
  using Consumer = std::function<void(Type)>;
  using type = Type;

  const std::optional<Type>& Get() const { return data_->value; }

  void SetConsumer(Consumer consumer) {
    CHECK(!data_->consumer.has_value());
    if (data_->value.has_value()) {
      consumer(std::move(data_->value.value()));
      data_->consumer = nullptr;
      data_->value = std::nullopt;
    } else {
      data_->consumer = std::move(consumer);
    }
  }

 private:
  friend Future<Type>;

  struct FutureData {
    // std::nullopt before a consumer is set. nullptr when a consumer has
    // already been executed.
    std::optional<Consumer> consumer;
    std::optional<Type> value;
  };

  Value(std::shared_ptr<FutureData> data) : data_(std::move(data)) {}

  std::shared_ptr<FutureData> data_ = std::make_shared<FutureData>();
};

template <typename Type>
struct Future {
 public:
  Future() : Future(std::make_shared<FutureData>()) {}

  typename Value<Type>::Consumer consumer;
  Value<Type> value;

 private:
  using FutureData = typename Value<Type>::FutureData;

  Future(std::shared_ptr<FutureData> data)
      : consumer([data](Type value) {
          CHECK(!data->value.has_value());
          CHECK(!data->consumer.has_value() ||
                data->consumer.value() != nullptr);
          if (data->consumer.has_value()) {
            data->consumer.value()(std::move(value));
            data->consumer = nullptr;
          } else {
            data->value.emplace(std::move(value));
          }
        }),
        value(std::move(data)) {}
};

template <typename Type>
static Value<Type> Past(Type value) {
  Future<Type> output;
  output.consumer(std::move(value));
  return output.value;
}

enum class IterationControlCommand { kContinue, kStop };

// Evaluate `callable` for each element in the range [begin, end). `callable`
// receives a reference to each element and must return a
// Value<IterationControlCommand>.
//
// The returned value can be used to check whether the entire evaluation
// succeeded and/or to detect when it's finished.
template <typename Iterator, typename Callable>
Value<IterationControlCommand> ForEach(Iterator begin, Iterator end,
                                       Callable callable) {
  Future<IterationControlCommand> output;
  auto resume = [consumer = output.consumer, end, callable](
                    Iterator begin, auto resume) mutable {
    if (begin == end) {
      consumer(IterationControlCommand::kContinue);
      return;
    }
    callable(*begin).SetConsumer([consumer, begin, end, callable, resume](
                                     IterationControlCommand result) mutable {
      if (result == IterationControlCommand::kStop) {
        consumer(result);
      } else {
        resume(++begin, resume);
      }
    });
  };
  resume(begin, resume);
  return output.value;
}

template <typename Callable>
Value<IterationControlCommand> While(Callable callable) {
  Future<IterationControlCommand> output;
  auto resume = [consumer = output.consumer,
                 callable](auto resume) mutable -> void {
    callable().SetConsumer([consumer = std::move(consumer),
                            callable = std::move(callable),
                            resume](IterationControlCommand result) mutable {
      if (result == IterationControlCommand::kStop) {
        consumer(result);
      } else {
        resume(resume);
      }
    });
  };
  resume(resume);
  return output.value;
}

template <typename OtherType, typename Callable>
auto Transform(Value<OtherType> delayed_value, Callable callable) {
  Future<typename decltype(callable(std::declval<OtherType>()))::type> output;
  delayed_value.SetConsumer(
      [consumer = output.consumer,
       callable = std::move(callable)](OtherType other_value) mutable {
        callable(std::move(other_value)).SetConsumer(std::move(consumer));
      });
  return output.value;
}

template <typename OtherType, typename Callable>
auto ImmediateTransform(Value<OtherType> delayed_value, Callable callable) {
  using Type = decltype(callable(std::declval<OtherType>()));
  Future<Type> output;
  delayed_value.SetConsumer(
      [consumer = output.consumer,
       callable = std::move(callable)](OtherType other_value) mutable {
        Type type = callable(std::move(other_value));
        consumer(std::move(type));
      });
  return output.value;
}

}  // namespace afc::futures

#endif  // __AFC_EDITOR_FUTURES_FUTURES_H__
