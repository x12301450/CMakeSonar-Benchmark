#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>

#include "megbrain/utils/mempool.h"
#include "megbrain/utils/metahelper.h"

namespace mgb::imperative {

template <typename T>
class MemPoolUtils {
private:
    static std::mutex sm_mutex;
    static std::unordered_map<std::thread::id, std::unique_ptr<MemPool<T>>>
            sm_instances;
    static thread_local MemPool<T>* tm_instance;
    static MemPool<T>* sm_instance;

public:
    static MemPool<T>& get_thread_local() {
        if (!tm_instance) {
            MGB_LOCK_GUARD(sm_mutex);
            auto& instance = sm_instances[std::this_thread::get_id()];
            if (!instance) {  // thread id may be duplicated
                instance = std::make_unique<MemPool<T>>();
            }
            tm_instance = instance.get();
        }
        return *tm_instance;
    }
    static MemPool<T>& get_static() {
        if (!sm_instance) {
            MGB_LOCK_GUARD(sm_mutex);
            auto& instance = sm_instances[{}];
            if (!instance) {  // double check
                instance = std::make_unique<MemPool<T>>();
                sm_instance = instance.get();
            }
        }
        return *sm_instance;
    }
};

template <typename T>
std::mutex MemPoolUtils<T>::sm_mutex;

template <typename T>
std::unordered_map<std::thread::id, std::unique_ptr<MemPool<T>>>
        MemPoolUtils<T>::sm_instances;

template <typename T>
thread_local MemPool<T>* MemPoolUtils<T>::tm_instance = nullptr;

template <typename T>
MemPool<T>* MemPoolUtils<T>::sm_instance = nullptr;

}  // namespace mgb::imperative
