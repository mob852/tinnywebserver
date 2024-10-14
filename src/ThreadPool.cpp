#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t thread_count) : stop_(false) {
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(std::thread(&ThreadPool::worker, this));
    }
}

ThreadPool::~ThreadPool() {
    stop_.store(true);
    condition_.notify_all();
    for (std::thread &worker : workers_) {
        if (worker.joinable())
            worker.join();
    }
}

void ThreadPool::worker() {
    while (!stop_) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty())
                return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            // 将线程ID转换为字符串
            std::ostringstream oss_start;
            oss_start << "工作线程 " << std::this_thread::get_id() << " 开始执行任务。";
            LOG_DEBUG(oss_start.str());

            task(); // 执行任务

            std::ostringstream oss_end;
            oss_end << "工作线程 " << std::this_thread::get_id() << " 完成任务。";
            LOG_DEBUG(oss_end.str());
        } catch (const std::exception& e) {
            LOG_ERROR("工作线程发生异常：" + std::string(e.what()));
        }
    
    }
}

