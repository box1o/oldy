#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <utility>
#include <algorithm>
#include <condition_variable>

#include "woki/task/executor.hpp"

namespace woki::task {

namespace {

void Reject(std::deque<detail::Work> work, ErrorCode code, std::string_view message) noexcept {
    for (auto& item : work) {
        if (item.reject) {
            try {
                item.reject(MakeError(code, message));
            } catch (...) {
            }
        }
    }
}

#ifndef __EMSCRIPTEN__
class ThreadPool {
public:
    ThreadPool(std::size_t worker_count, std::size_t capacity)
        : state_(std::make_shared<State>(std::max<std::size_t>(1, capacity))) {
        worker_count = std::max<std::size_t>(1, worker_count);
        threads_.reserve(worker_count);
        for (std::size_t index = 0; index < worker_count; ++index) {
            threads_.emplace_back([state = state_] { Worker(std::move(state)); });
        }
    }

    ~ThreadPool() {
        RequestStop();
        Join();
    }

    Result<void> Submit(detail::Work work) {
        std::lock_guard lock(state_->mutex);
        if (state_->stopping) {
            return Err(ErrorCode::ExecutorStopped, "executor is stopped");
        }
        if (state_->queue.size() >= state_->capacity) {
            return Err(ErrorCode::QueueFull, "executor queue is full");
        }
        state_->queue.emplace_back(std::move(work));
        state_->available.notify_one();
        return Ok();
    }

    void RequestStop() noexcept {
        std::deque<detail::Work> abandoned;
        {
            std::lock_guard lock(state_->mutex);
            if (state_->stopping) {
                return;
            }
            state_->stopping = true;
            abandoned.swap(state_->queue);
        }
        state_->available.notify_all();
        Reject(std::move(abandoned), ErrorCode::ExecutorStopped, "executor stopped before task execution");
    }

    void Join() noexcept {
        const auto self = std::this_thread::get_id();
        for (auto& thread : threads_) {
            if (!thread.joinable()) {
                continue;
            }
            if (thread.get_id() == self) {
                thread.detach();
            } else {
                thread.join();
            }
        }
    }

    bool IsStopped() const noexcept {
        std::lock_guard lock(state_->mutex);
        return state_->stopping;
    }

    std::size_t WorkerCount() const noexcept {
        return threads_.size();
    }

private:
    struct State {
        explicit State(std::size_t queue_capacity)
            : capacity(queue_capacity) {}

        std::mutex mutex;
        std::condition_variable available;
        std::deque<detail::Work> queue;
        std::size_t capacity;
        bool stopping = false;
    };

    static void Worker(std::shared_ptr<State> state) noexcept {
        for (;;) {
            detail::Work work;
            {
                std::unique_lock lock(state->mutex);
                state->available.wait(lock, [&] { return state->stopping || !state->queue.empty(); });
                if (state->queue.empty()) {
                    return;
                }
                work = std::move(state->queue.front());
                state->queue.pop_front();
            }
            try {
                work.run();
            } catch (...) {
            }
        }
    }

    std::shared_ptr<State> state_;
    std::vector<std::thread> threads_;
};
#else
class ThreadPool {
public:
    ThreadPool(std::size_t worker_count, std::size_t capacity) {}

    Result<void> Submit(detail::Work work) {
        if (stopped_) {
            return Err(ErrorCode::ExecutorStopped, "executor is stopped");
        }
        try {
            work.run();
        } catch (...) {
        }
        return Ok();
    }

    void RequestStop() noexcept {
        stopped_ = true;
    }

    void Join() noexcept {}

    bool IsStopped() const noexcept {
        return stopped_;
    }

    std::size_t WorkerCount() const noexcept {
        return 1;
    }

private:
    bool stopped_ = false;
};
#endif

} // namespace

Result<void> Executor::Submit(Job job) {
    if (!job) {
        return Err(ErrorCode::InvalidArgument, "executor job is empty");
    }
    return SubmitWork(detail::Work{std::move(job), {}});
}

Result<void> detailSubmit(Executor& executor, detail::Work work) {
    if (!work.run) {
        return Err(ErrorCode::InvalidArgument, "executor job is empty");
    }
    return executor.SubmitWork(std::move(work));
}

class Scheduler::Impl : public ThreadPool {
public:
    using ThreadPool::ThreadPool;
};

Scheduler::Scheduler(std::size_t worker_count, std::size_t queue_capacity)
    : impl_(
          std::make_unique<Impl>(
              worker_count == 0 ? std::max(1u, std::thread::hardware_concurrency()) : worker_count,
              queue_capacity
          )
      ) {}

Scheduler::~Scheduler() = default;

void Scheduler::RequestStop() noexcept {
    impl_->RequestStop();
}

void Scheduler::Join() noexcept {
    impl_->Join();
}

bool Scheduler::IsStopped() const noexcept {
    return impl_->IsStopped();
}

std::size_t Scheduler::WorkerCount() const noexcept {
    return impl_->WorkerCount();
}

Result<void> Scheduler::SubmitWork(detail::Work work) {
    return impl_->Submit(std::move(work));
}

class IoExecutor::Impl : public ThreadPool {
public:
    using ThreadPool::ThreadPool;
};

IoExecutor::IoExecutor(std::size_t worker_count, std::size_t queue_capacity)
    : impl_(std::make_unique<Impl>(std::max<std::size_t>(1, worker_count), queue_capacity)) {}

IoExecutor::~IoExecutor() = default;

void IoExecutor::RequestStop() noexcept {
    impl_->RequestStop();
}

void IoExecutor::Join() noexcept {
    impl_->Join();
}

bool IoExecutor::IsStopped() const noexcept {
    return impl_->IsStopped();
}

std::size_t IoExecutor::WorkerCount() const noexcept {
    return impl_->WorkerCount();
}

Result<void> IoExecutor::SubmitWork(detail::Work work) {
    return impl_->Submit(std::move(work));
}

class CompletionQueue::Impl {
public:
    explicit Impl(std::size_t queue_capacity)
        : capacity(std::max<std::size_t>(1, queue_capacity)) {}

    std::mutex mutex;
    std::deque<detail::Work> queue;
    std::size_t capacity;
    bool stopped = false;
};

CompletionQueue::CompletionQueue(std::size_t capacity)
    : impl_(std::make_unique<Impl>(capacity)) {}

CompletionQueue::~CompletionQueue() {
    RequestStop();
}

Result<void> CompletionQueue::SubmitWork(detail::Work work) {
    std::lock_guard lock(impl_->mutex);
    if (impl_->stopped) {
        return Err(ErrorCode::ExecutorStopped, "completion queue is stopped");
    }
    if (impl_->queue.size() >= impl_->capacity) {
        return Err(ErrorCode::QueueFull, "completion queue is full");
    }
    impl_->queue.emplace_back(std::move(work));
    return Ok();
}

std::size_t CompletionQueue::Drain(std::size_t limit) {
    std::size_t drained = 0;
    while (drained < limit) {
        detail::Work work;
        {
            std::lock_guard lock(impl_->mutex);
            if (impl_->queue.empty()) {
                break;
            }
            work = std::move(impl_->queue.front());
            impl_->queue.pop_front();
        }
        try {
            work.run();
        } catch (...) {
        }
        ++drained;
    }
    return drained;
}

void CompletionQueue::RequestStop() noexcept {
    std::deque<detail::Work> abandoned;
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->stopped) {
            return;
        }
        impl_->stopped = true;
        abandoned.swap(impl_->queue);
    }
    Reject(std::move(abandoned), ErrorCode::ExecutorStopped, "completion queue stopped before publication");
}

bool CompletionQueue::IsStopped() const noexcept {
    std::lock_guard lock(impl_->mutex);
    return impl_->stopped;
}

} // namespace woki::task
