#pragma once

#include "../../asio_include.h"
#include <net/quic/quartc/quartc_task_runner_interface.h>

namespace fast_asio {
namespace quic {
namespace detail {

using namespace net;

class task_runner_service
    : public QuartcTaskRunnerInterface,
    public boost::asio::detail::service_base<task_runner_service>
{
public:
    typedef boost::asio::steady_timer timer_type;

    explicit task_runner_service(boost::asio::io_context & ioc)
        : boost::asio::detail::service_base<task_runner_service>(ioc)
    {
    }

    virtual ~task_runner_service() {}

    // A handler used to cancel a scheduled task. In some cases, a task cannot
    // be directly canceled with its pointer. For example, in WebRTC, the task
    // will be scheduled on rtc::Thread. When canceling a task, its pointer cannot
    // locate the scheduled task in the thread message queue. So when scheduling a
    // task, an additional handler (ScheduledTask) will be returned.
    class ScheduledTask
        : public QuartcTaskRunnerInterface::ScheduledTask
    {
    public:
        timer_type timer_;
        Task* task_;

        ScheduledTask(boost::asio::io_context & ioc, Task* task)
            : timer(ioc), task_(task)
        {
        }

        // Cancels a scheduled task, meaning the task will not be run.
        virtual void Cancel() {
            boost::system::error_code ignore_ec;
            timer_.cancel(ignore_ec);
        }
    };

    // Schedules a task, which will be run after the given delay. A ScheduledTask
    // may be used to cancel the task.
    std::unique_ptr<QuartcTaskRunnerInterface::ScheduledTask>
        Schedule(Task* task, uint64_t delay_ms) 
    {
        ScheduledTask* scheduled_task_ptr = new ScheduledTask(get_io_context(), task);
        scheduled_task_ptr->timer_.expires_after(std::chrono::milliseconds(delay_ms));
        scheduled_task_ptr->timer_.async_wait([task](boost::system::error_code ec){
                    if (!ec) task->Run();
                });
        return scheduled_task_ptr;
    }
};

} // namespace detail
} // namespace quic
} // namespace fast_asio

