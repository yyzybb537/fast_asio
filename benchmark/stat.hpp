#pragma once
#include <iostream>
#include <thread>
#include <cstddef>
#include <array>
#include <chrono>

class stat {
    std::thread thread_;

public:
    enum stat_category {
        qps,
        bytes,
        max_stat_category,
    };

    typedef std::array<uint64_t, max_stat_category> values_type;

public:
    static stat & instance() {
        static stat obj;
        return obj;
    }

    static values_type & get_values() {
        static thread_local
    }

    void start() {
        std::thread thread(&stat::run, &getInstance());
        thread_.swap(thread);
    }

    void run() {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            show();
        }
    }

    void show() {

    }

private:
    stat() = default;
    stat(stat const&) = delete;
    stat(stat &&) = delete;
};
