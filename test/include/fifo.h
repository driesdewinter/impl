#ifndef TEST_FIFO_H_
#define TEST_FIFO_H_

#include <atomic>
#include <utility>
#include <cstdint>
#include <type_traits>

template<typename T, std::size_t N = 65536, std::size_t B = 256>
struct fifo
{
  typename std::aligned_storage<sizeof(T), alignof(T)>::type data[N];
  uint8_t padding1[64];
  std::size_t head = 0;
  uint8_t padding2[64];
  std::size_t new_head = 0;
  uint8_t padding3[64];
  std::size_t tail = 0;
  uint8_t padding4[64];
  std::size_t new_tail = 0;

  bool full() const
  {
    return (new_tail + 1) % N == head;
  }

  template<typename... Args>
  void push(Args&&... args)
  {
    new (&data[new_tail]) T(std::forward<Args>(args)...);
    new_tail = (new_tail + 1) % N;
    std::size_t taildiff = (N + new_tail - tail) % N;
    if (taildiff >= B) flush();
  }

  void flush()
  {
    atomic_thread_fence(std::memory_order_release);
    tail = new_tail;
  }

  bool empty() const
  {
    return new_head == tail;
  }

  T& front()
  {
    atomic_thread_fence(std::memory_order_acquire);
    return *reinterpret_cast<T*>(&data[new_head]);
  }

  void pop()
  {
    atomic_thread_fence(std::memory_order_acquire);
    reinterpret_cast<T*>(&data[new_head])->~T();
    new_head = (new_head + 1) % N;
    std::size_t headdiff = (N + new_head - head) % N;
    if (headdiff >= B)
    {
      head = new_head;
    }
  }
};


#endif /* TEST_FIFO_H_ */
