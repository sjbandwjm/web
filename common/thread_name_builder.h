#pragma once
#include <string>

namespace holo
{
namespace winterfell
{

/**
 * @brief set thread name strategy builder
 *
 */
class ThreadNameBuilder
{
public:
  static constexpr int thread_name_capacity = 16;
  static constexpr int thread_name_max_len  = 15;
  ThreadNameBuilder(const char* class_name, const char* func_name, const char* explain = "");
  /**
   * @brief return thread name composed of construct params
   *
   * @return const std::string&
   */
  const std::string& Name() const;

private:
  bool isStrEmpty(const char* str);

  bool isNameLengthValid(int free_len = 0);

  const std::string demangle(const char* name);

  void tryTruncate(int max_len);

  std::string thread_name_;
};

}  // namespace winterfell
}  // namespace holo

#define HOLO_SET_THREAD_NAME_INTRENAL(class_name, func_name, explain)                                                  \
    {                                                                                                                  \
        const static holo::winterfell::ThreadNameBuilder holo_set_thread_name_builder(class_name, func_name, explain); \
        pthread_setname_np(pthread_self(), holo_set_thread_name_builder.Name().c_str());                               \
    }

#define HOLO_SET_THREAD_NAME_MEMBER_FUNC(explain)                                                                      \
    HOLO_SET_THREAD_NAME_INTRENAL(typeid(*this).name(), __FUNCTION__, #explain)
#define HOLO_SET_THREAD_NAME_STATIC_FUNC(explain) HOLO_SET_THREAD_NAME_INTRENAL("", __FUNCTION__, #explain)
