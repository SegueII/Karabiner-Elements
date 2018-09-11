#define CATCH_CONFIG_MAIN
#include "../../vendor/catch/catch.hpp"

#include "thread_utility.hpp"

TEST_CASE("initialize") {
  krbn::thread_utility::register_main_thread();
}

TEST_CASE("wait") {
  std::cout << "wait" << std::endl;

  for (int i = 0; i < 1000; ++i) {
    krbn::thread_utility::wait wait;
    std::atomic<int> count(0);

    std::thread thread([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

      REQUIRE(count == 0);
      wait.notify();
    });

    wait.wait_notice();
    count = 1;

    thread.join();
  }
}

TEST_CASE("timer") {
  std::cout << "timer" << std::endl;

  {
    size_t count = 0;

    krbn::thread_utility::timer timer(
        std::chrono::milliseconds(100),
        krbn::thread_utility::timer::mode::once,
        [&] {
          ++count;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // `cancel` is ignored.
    timer.cancel();

    REQUIRE(count == 1);
  }

  // Wait

  {
    size_t count = 0;
    size_t wait_count = 0;

    krbn::thread_utility::timer timer(
        std::chrono::milliseconds(500),
        krbn::thread_utility::timer::mode::once,
        [&] {
          ++count;
        });

    auto thread = std::thread([&] {
      for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ++wait_count;
      }
    });

    timer.wait();

    REQUIRE(count == 1);
    REQUIRE(wait_count > 1);

    thread.join();
  }

  // Wait in destructor

  {
    size_t count = 0;

    {
      krbn::thread_utility::timer timer(
          std::chrono::milliseconds(500),
          krbn::thread_utility::timer::mode::once,
          [&] {
            ++count;
          });
    }

    REQUIRE(count == 1);
  }

  // Cancel

  {
    size_t count = 0;

    krbn::thread_utility::timer timer(
        std::chrono::milliseconds(500),
        krbn::thread_utility::timer::mode::once,
        [&] {
          ++count;
        });

    timer.cancel();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    REQUIRE(count == 0);
  }

  // Cancel while wait

  {
    size_t count = 0;

    krbn::thread_utility::timer timer(
        std::chrono::milliseconds(500),
        krbn::thread_utility::timer::mode::once,
        [&] {
          ++count;
        });

    auto thread = std::thread([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      timer.cancel();
    });

    timer.wait();

    REQUIRE(count == 0);

    thread.join();
  }
}

TEST_CASE("timer.repeats") {
  std::cout << "timer.repeats" << std::endl;

  // Cancel while wait

  {
    size_t count = 0;

    krbn::thread_utility::timer timer(
        std::chrono::milliseconds(100),
        krbn::thread_utility::timer::mode::repeat,
        [&] {
          ++count;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    timer.cancel();
    timer.wait();

    REQUIRE(count >= 2);
    REQUIRE(count <= 10);
  }

  {
    size_t count = 0;

    // Cancel repeat in `~timer`.

    {
      krbn::thread_utility::timer timer(
          std::chrono::milliseconds(100),
          krbn::thread_utility::timer::mode::repeat,
          [&] {
            ++count;
          });

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    REQUIRE(count >= 2);
    REQUIRE(count <= 10);
  }

  {
    size_t count = 0;

    // The callback is called in ~timer.

    {
      krbn::thread_utility::timer timer(
          std::chrono::milliseconds(100),
          krbn::thread_utility::timer::mode::repeat,
          [&] {
            ++count;
          });
    }

    REQUIRE(count == 1);
  }

  {
    size_t count = 0;

    // `cancel`

    {
      krbn::thread_utility::timer timer(
          std::chrono::milliseconds(100),
          krbn::thread_utility::timer::mode::repeat,
          [&] {
            ++count;
            if (count == 2) {
              timer.cancel();
            }
          });

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    REQUIRE(count == 2);
  }
}

TEST_CASE("timer.repeats.interval") {
  std::cout << "timer.repeats.interval" << std::endl;

  {
    size_t count = 0;

    krbn::thread_utility::timer timer(
        [](auto&& count) {
          if (count == 0) {
            return std::chrono::milliseconds(0);
          } else {
            return std::chrono::milliseconds(500);
          }
        },
        krbn::thread_utility::timer::mode::repeat,
        [&] {
          ++count;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(count == 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    REQUIRE(count > 1);
  }
}

TEST_CASE("dispatcher") {
  std::cout << "dispatcher" << std::endl;

  {
    size_t count = 0;

    krbn::thread_utility::dispatcher dispatcher;

    for (int i = 0; i < 10000; ++i) {
      dispatcher.enqueue([&count, i] {
        ++count;
        if (i % 1000 == 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
      });
    }

    REQUIRE(count < 10000);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    REQUIRE(count == 10000);

    dispatcher.terminate();
  }

  // Preserve the order of entries.

  {
    std::string text;

    krbn::thread_utility::dispatcher dispatcher;

    dispatcher.enqueue([&] {
      text += "a";
    });
    dispatcher.enqueue([&] {
      text += "b";
    });
    dispatcher.enqueue([&] {
      text += "c";
    });

    dispatcher.terminate();

    REQUIRE(text == "abc");
  }

  // Run enqueued functions in the destructor.

  {
    size_t count = 0;

    {
      krbn::thread_utility::dispatcher dispatcher;

      for (int i = 0; i < 10000; ++i) {
        dispatcher.enqueue([&count, i] {
          ++count;
          if (i % 1000 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
          }
        });
      }

      dispatcher.terminate();
    }

    REQUIRE(count == 10000);
  }

  // Ignore `enqueue` after `terminate`.

  {
    krbn::thread_utility::dispatcher dispatcher;

    dispatcher.terminate();

    dispatcher.enqueue([&dispatcher] {
      dispatcher.enqueue([] {
      });
    });
  }
}

namespace {
void dispatcher_recursive_function(krbn::thread_utility::dispatcher& dispatcher,
                                   size_t& count) {
  ++count;
  if (count < 5) {
    dispatcher.enqueue([&dispatcher, &count] {
      dispatcher_recursive_function(dispatcher, count);
    });
  } else if (count == 5) {
    dispatcher.enqueue([] {
      std::cout << "dispatcher_recursive_function finished" << std::endl;
    });
  }
}

class dispatcher_recursive_class final {
public:
  dispatcher_recursive_class(size_t& count) : count_(count) {
    dispatcher_ = std::make_unique<krbn::thread_utility::dispatcher>();
  }

  ~dispatcher_recursive_class(void) {
    dispatcher_->terminate();
    dispatcher_ = nullptr;
  }

  void enqueue(void) {
    dispatcher_->enqueue([this] {
      dispatcher_->enqueue([this] {
        ++count_;
        std::cout << "dispatcher_recursive_class finished" << std::endl;
      });
    });
  }

private:
  size_t& count_;

  std::unique_ptr<krbn::thread_utility::dispatcher> dispatcher_;
};
} // namespace

TEST_CASE("dispatcher.recursive") {
  std::cout << "dispatcher.recursive" << std::endl;

  // Call `enqueue` in dispatcher's thread.

  {
    size_t count = 0;

    {
      krbn::thread_utility::dispatcher dispatcher;

      dispatcher.enqueue([&] {
        dispatcher_recursive_function(dispatcher, count);
      });

      dispatcher.terminate();
    }

    REQUIRE(count == 5);
  }

  {
    size_t count = 0;

    {
      dispatcher_recursive_class dispatcher_recursive_class(count);

      dispatcher_recursive_class.enqueue();
    }

    REQUIRE(count == 1);
  }
}

namespace {
class dispatcher_enqueue_and_wait final {
public:
  dispatcher_enqueue_and_wait(size_t& count) : count_(count) {
    dispatcher_ = std::make_shared<krbn::thread_utility::dispatcher>();
    weak_dispatcher_ = dispatcher_;
  }

  ~dispatcher_enqueue_and_wait(void) {
    dispatcher_->terminate();
    dispatcher_ = nullptr;
  }

  void enqueue(void) {
    if (auto d = weak_dispatcher_.lock()) {
      d->enqueue([this] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++count_;
        enqueue();
      });
    }
  }

  void enqueue_and_wait(void) {
    if (auto d = weak_dispatcher_.lock()) {
      weak_dispatcher_.reset();

      d->enqueue_and_wait([] {});
    }
  }

private:
  size_t& count_;

  std::shared_ptr<krbn::thread_utility::dispatcher> dispatcher_;
  std::weak_ptr<krbn::thread_utility::dispatcher> weak_dispatcher_;
};
} // namespace

TEST_CASE("dispatcher.enqueue_and_wait") {
  std::cout << "dispatcher.enqueue_and_wait" << std::endl;

  size_t count = 0;
  {
    dispatcher_enqueue_and_wait dispatcher_enqueue_and_wait(count);
    dispatcher_enqueue_and_wait.enqueue();
    dispatcher_enqueue_and_wait.enqueue_and_wait();
    REQUIRE(count == 1);

    // `enqueue` ignored after `weak_dispatcher.reset()`.
    dispatcher_enqueue_and_wait.enqueue();
  }
  REQUIRE(count == 1);
}
