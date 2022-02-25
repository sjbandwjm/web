#pragma once

#include <type_traits>
#include <utility>

#include "bthread/bthread.h"

namespace jiayou::base {

typedef void*(*ThreadEntry)(void*);
typedef void(*VoidThreadEntry)(void*);

template <class Fn>
class ThunkEntry {
 public:
  ThunkEntry(Fn&& fn) : fn_(fn) {}

  static void* Call(void* data) {
    auto* entry = static_cast<ThunkEntry<Fn>*>(data);
    entry->fn_();
    delete entry;
    return nullptr;
  }

  static void Call2(void* data) {
    auto* entry = static_cast<ThunkEntry<Fn>*>(data);
    entry->fn_();
    delete entry;
  }

  static constexpr VoidThreadEntry timer_entry() {
    return &ThunkEntry<Fn>::Call2;
  }

  static constexpr VoidThreadEntry ventry() {
    return &ThunkEntry<Fn>::Call2;
  }

  static constexpr ThreadEntry entry() {
    return &ThunkEntry<Fn>::Call;
  }

 private:
  Fn fn_;
};

template <class Lambda>
class LambdaEntryThunk {
 public:
  static void Call() {
    (*lambda_)();
    delete lambda_;
  }

 private:
  static Lambda* lambda_;
};

template <class Lambda>
Lambda* LambdaEntryThunk<Lambda>::lambda_ = nullptr;

typedef void(*ThunkFunctionType)();

template <class Lambda>
ThunkFunctionType LambdaEntry(Lambda&& lambda) {
  LambdaEntryThunk<typename std::remove_reference<Lambda>::type>::lambda_ = new Lambda(std::move(lambda));
  return &LambdaEntryThunk<typename std::remove_reference<Lambda>::type>::Call;
}

template <class Fn>
void BthreadStartBackground(Fn&& fn) {
  auto* entry = new jiayou::base::ThunkEntry(std::move(fn));
  bthread_t bid;
  bthread_start_background(&bid, nullptr, entry->entry(), entry);
}

}
