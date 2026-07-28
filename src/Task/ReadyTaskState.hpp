#pragma once

#include <scorch/server/Task.hpp>

#include <coroutine>
#include <memory>

template<typename T>
class ReadyTaskState : public scorch::server::TaskState<T>
{
public:
    ReadyTaskState(T value)
        : m_value(std::move(value))
    {
    }

    bool await_ready() const noexcept override
    {
        return true;
    }

    void await_suspend(std::coroutine_handle<>) override
    {
    }

    T await_resume() override
    {
        return std::move(m_value);
    }

private:
    T m_value;
};

template<typename T>
scorch::server::Task<T> MakeReadyTask(T value)
{
    return scorch::server::Task<T>(
        std::make_shared<ReadyTaskState<T>>(std::move(value))
    );
}