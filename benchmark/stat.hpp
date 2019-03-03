#pragma once
#include <stdio.h>
#include <thread>
#include <cstddef>
#include <array>
#include <chrono>
#include <string>
#include <mutex>
#include <functional>

class stats {

public:
    enum stats_category {
        qps,
        bytes,
        max_stats_category,
    };

    const char* category_name(int i) {
#define DEF_CATEGORY_NAME(e) case e: return #e
        switch(i) {
            DEF_CATEGORY_NAME(qps);
            DEF_CATEGORY_NAME(bytes);
        }
#undef DEF_CATEGORY_NAME
        return "Unkown Stats Category";
    }

    typedef std::array<int64_t, max_stats_category> values_type;

    std::mutex mtx_;
    std::vector<values_type*> values_list_;
    std::function<std::string()> message_functor_;

public:
    static stats & instance() {
        static stats obj;
        return obj;
    }

    void set_message_functor(std::function<std::string()> fn) {
        message_functor_ = fn;
    }

    void inc(int type, int64_t val) {
        get_values()[type] += val;
    }

    int64_t get(int type) {
        int64_t val = 0;
        std::unique_lock<std::mutex> lock(mtx_);
        for (auto & values_ptr : values_list_)
            val += (*values_ptr)[type];
        return val;
    }

private:
    static void thread_run() {
        instance().run();
    }

    void run() {
        values_type last {};
        values_type this_second {};

        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            for (int i = 0; i < max_stats_category; ++i) {
                this_second[i] = get(i);
            }

            for (int i = 0; i < max_stats_category; ++i) {
                printf("%s: %s | ", category_name(i), integer2str(this_second[i] - last[i]).c_str());
            }
            if (message_functor_)
                printf("%s |", message_functor_().c_str());
            printf("\n");

            std::swap(last, this_second);
        }
    }

    static values_type & get_values() {
        static thread_local values_type* this_thread_values = instance().init_values();
        return *this_thread_values;
    }

    values_type* init_values() {
        values_type* values = new values_type{};
        std::unique_lock<std::mutex> lock(mtx_);
        values_list_.push_back(values);
        return values;
    }

    std::string integer2str(int64_t val) {
        char buf[64];
        if (val > 1024 * 1024 * 1024) {
            // GB
            double f64_val = (double)val / (1024 * 1024 * 1024);
            snprintf(buf, sizeof(buf), "%.2f G", f64_val);
            return std::string(buf);
        } else if (val > 1024 * 1024) {
            // GB
            double f64_val = (double)val / (1024 * 1024);
            snprintf(buf, sizeof(buf), "%.2f M", f64_val);
            return std::string(buf);
        } else if (val > 1024) {
            // GB
            double f64_val = (double)val / (1024);
            snprintf(buf, sizeof(buf), "%.2f K", f64_val);
            return std::string(buf);
        }

        snprintf(buf, sizeof(buf), "%ld", val);
        return std::string(buf);
    }

private:
    stats() {
        std::thread thread(&stats::thread_run);
        thread.detach();
    }
    stats(stats const&) = delete;
    stats(stats &&) = delete;
};
