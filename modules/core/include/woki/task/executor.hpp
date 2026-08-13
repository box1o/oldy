#pragma once

// IWYU pragma: private, include "woki/task.hpp"

#include <memory>
#include <cstddef>
#include <functional>

#include "woki/error/result.hpp"

namespace woki::task {

namespace detail {
struct Work {
    std::function<void()> run;
    std::function<void(Error)> reject;
};
} // namespace detail

class Executor {
public:
    using Job = std::function<void()>;

    virtual ~Executor() = default;

    [[nodiscard]] Result<void> Submit(Job job);
    [[nodiscard]] virtual bool IsStopped() const noexcept = 0;

protected:
    [[nodiscard]] virtual Result<void> SubmitWork(detail::Work work) = 0;

    friend Result<void> detailSubmit(Executor&, detail::Work);
};

[[nodiscard]] Result<void> detailSubmit(Executor& executor, detail::Work work);

class Scheduler final : public Executor {
public:
    explicit Scheduler(std::size_t worker_count = 0, std::size_t queue_capacity = 1024);
    ~Scheduler() override;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    void RequestStop() noexcept;
    void Join() noexcept;
    [[nodiscard]] bool IsStopped() const noexcept override;
    [[nodiscard]] std::size_t WorkerCount() const noexcept;

protected:
    [[nodiscard]] Result<void> SubmitWork(detail::Work work) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class IoExecutor final : public Executor {
public:
    explicit IoExecutor(std::size_t worker_count = 1, std::size_t queue_capacity = 256);
    ~IoExecutor() override;

    IoExecutor(const IoExecutor&) = delete;
    IoExecutor& operator=(const IoExecutor&) = delete;

    void RequestStop() noexcept;
    void Join() noexcept;
    [[nodiscard]] bool IsStopped() const noexcept override;
    [[nodiscard]] std::size_t WorkerCount() const noexcept;

protected:
    [[nodiscard]] Result<void> SubmitWork(detail::Work work) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class CompletionQueue final : public Executor {
public:
    explicit CompletionQueue(std::size_t capacity = 1024);
    ~CompletionQueue() override;

    CompletionQueue(const CompletionQueue&) = delete;
    CompletionQueue& operator=(const CompletionQueue&) = delete;

    [[nodiscard]] std::size_t Drain(std::size_t limit = static_cast<std::size_t>(-1));
    void RequestStop() noexcept;
    [[nodiscard]] bool IsStopped() const noexcept override;

protected:
    [[nodiscard]] Result<void> SubmitWork(detail::Work work) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace woki::task
