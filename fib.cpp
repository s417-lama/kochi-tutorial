#include <iostream>
#include <chrono>

#include <mtbb/task_group.h>

class measure_time {
  std::chrono::time_point<std::chrono::system_clock> t_start_;
public:
  measure_time() {
    t_start_ = std::chrono::system_clock::now();
  }
  ~measure_time() {
    std::chrono::nanoseconds d = std::chrono::system_clock::now() - t_start_;
    std::cout << "Execution Time: " << d.count() << " ns" << std::endl;
  }
};

int fib(int n) {
  if (n < 2) {
    return n;
  } else {
    int x, y;
    mtbb::task_group tg;
    tg.run([=, &x] { x = fib(n - 1); });
    y = fib(n - 2);
    tg.wait();
    return x + y;
  }
}

int main(int argc, char** argv) {
  int n = (argc > 1 ? atoi(argv[1]) : 35);
  {
    measure_time mt;
    int x = fib(n);
    std::cout << "fib(" << n << ") = " << x << std::endl;
  }
}
