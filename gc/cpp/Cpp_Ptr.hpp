
#pragma once

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <functional>
#include <string>

namespace GC {

    template<typename T> class Ptr;

    template<typename T>
    class ControlBlock {
    private:
        std::atomic<size_t> gc_strong_count_;
        std::atomic<size_t> gc_weak_count_;
        std::atomic<T*> ptr_;
        std::atomic<bool> object_destroyed_;

    public:
        explicit ControlBlock(T* p) noexcept
            : gc_strong_count_(1), gc_weak_count_(0), ptr_(p), object_destroyed_(false) {
        }

        ControlBlock(const ControlBlock&) = delete;
        ControlBlock& operator=(const ControlBlock&) = delete;

        void add_strong() noexcept {
            gc_strong_count_.fetch_add(1, std::memory_order_relaxed);
        }

        void add_weak() noexcept {
            gc_weak_count_.fetch_add(1, std::memory_order_relaxed);
        }

        bool try_add_strong() noexcept {
            size_t count = gc_strong_count_.load(std::memory_order_acquire);
            while (count > 0) {
                if (gc_strong_count_.compare_exchange_weak(
                    count, count + 1,
                    std::memory_order_acquire,
                    std::memory_order_acquire)) {
                    return true;
                }
            }
            return false;
        }

        void release_strong() noexcept {
            if (gc_strong_count_.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                destroy_object();
                if (gc_weak_count_.load(std::memory_order_acquire) == 0) {
                    std::atomic_thread_fence(std::memory_order_acquire);
                    delete this;
                }
            }
        }

        void release_weak() noexcept {
            if (gc_weak_count_.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                if (gc_strong_count_.load(std::memory_order_acquire) == 0) {
                    std::atomic_thread_fence(std::memory_order_acquire);
                    delete this;
                }
            }
        }

        T* get_ptr() const noexcept {
            return ptr_.load(std::memory_order_acquire);
        }

        bool is_alive() const noexcept {
            return gc_strong_count_.load(std::memory_order_acquire) > 0;
        }

        size_t strong_count() const noexcept {
            return gc_strong_count_.load(std::memory_order_acquire);
        }

        size_t weak_count() const noexcept {
            return gc_weak_count_.load(std::memory_order_acquire);
        }

    private:
        void destroy_object() noexcept {
            bool expected = false;
            if (object_destroyed_.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {

                T* p = ptr_.exchange(nullptr, std::memory_order_acq_rel);
                if (p) {
                    delete p;
                }
            }
        }
    };

    template<typename T>
    class Ptr {
    private:
        std::atomic<ControlBlock<T>*> ctrl_;
        std::atomic<bool> is_weak_;

        explicit Ptr(ControlBlock<T>* ctrl, bool is_weak) noexcept
            : ctrl_(ctrl), is_weak_(is_weak) {
        }

        template<typename U> friend class Ptr;

    public:
        constexpr Ptr() noexcept : ctrl_(nullptr), is_weak_(false) {}
        constexpr Ptr(std::nullptr_t) noexcept : ctrl_(nullptr), is_weak_(false) {}

        explicit Ptr(T* ptr) : ctrl_(nullptr), is_weak_(false) {
            if (ptr) {
                try {
                    ctrl_.store(new ControlBlock<T>(ptr), std::memory_order_release);
                }
                catch (...) {
                    delete ptr;
                    throw;
                }
            }
        }

        Ptr(const Ptr& other) noexcept : ctrl_(nullptr), is_weak_(false) {
            ControlBlock<T>* other_ctrl = other.ctrl_.load(std::memory_order_acquire);
            bool other_weak = other.is_weak_.load(std::memory_order_acquire);

            if (other_ctrl) {
                if (other_weak) {
                    other_ctrl->add_weak();
                }
                else {
                    other_ctrl->add_strong();
                }
            }
            ctrl_.store(other_ctrl, std::memory_order_release);
            is_weak_.store(other_weak, std::memory_order_release);
        }

        Ptr(Ptr&& other) noexcept
            : ctrl_(other.ctrl_.exchange(nullptr, std::memory_order_acq_rel)),
            is_weak_(other.is_weak_.exchange(false, std::memory_order_acq_rel)) {
        }

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        Ptr(const Ptr<U>& other) noexcept : ctrl_(nullptr), is_weak_(false) {
            auto other_ctrl = other.ctrl_.load(std::memory_order_acquire);
            bool other_weak = other.is_weak_.load(std::memory_order_acquire);

            ControlBlock<T>* converted_ctrl = reinterpret_cast<ControlBlock<T>*>(other_ctrl);

            if (converted_ctrl) {
                if (other_weak) {
                    converted_ctrl->add_weak();
                }
                else {
                    converted_ctrl->add_strong();
                }
            }

            ctrl_.store(converted_ctrl, std::memory_order_release);
            is_weak_.store(other_weak, std::memory_order_release);
        }

        ~Ptr() {
            release();
        }

        Ptr& operator=(const Ptr& other) noexcept {
            if (this != &other) {
                Ptr tmp(other);
                swap(tmp);
            }
            return *this;
        }

        Ptr& operator=(Ptr&& other) noexcept {
            if (this != &other) {
                release();
                ctrl_.store(other.ctrl_.exchange(nullptr, std::memory_order_acq_rel),
                    std::memory_order_release);
                is_weak_.store(other.is_weak_.exchange(false, std::memory_order_acq_rel),
                    std::memory_order_release);
            }
            return *this;
        }

        Ptr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        Ptr safe(const Ptr& strong_ref) const {
            ControlBlock<T>* ref_ctrl = strong_ref.ctrl_.load(std::memory_order_acquire);
            bool ref_weak = strong_ref.is_weak_.load(std::memory_order_acquire);

            if (!ref_ctrl || ref_weak) {
                return Ptr();
            }
            Ptr weak_ptr;
            weak_ptr.ctrl_.store(ref_ctrl, std::memory_order_release);
            weak_ptr.is_weak_.store(true, std::memory_order_release);
            ref_ctrl->add_weak();
            return weak_ptr;
        }

        Ptr lock() const {
            bool weak = is_weak_.load(std::memory_order_acquire);
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);

            if (!weak || !ctrl) {
                return Ptr(*this);
            }
            if (ctrl->try_add_strong()) {
                return Ptr(ctrl, false);
            }
            return Ptr();
        }

        void Ref(const Ptr& other) {
            if (this == &other) return;
            release();

            ControlBlock<T>* other_ctrl = other.ctrl_.load(std::memory_order_acquire);
            bool other_weak = other.is_weak_.load(std::memory_order_acquire);

            if (other_ctrl && !other_weak) {
                other_ctrl->add_weak();
                ctrl_.store(other_ctrl, std::memory_order_release);
                is_weak_.store(true, std::memory_order_release);
            }
        }

        bool expired() const noexcept {
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);
            return !ctrl || !ctrl->is_alive();
        }

        T* get() const noexcept {
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);
            if (!ctrl) {
                return nullptr;
            }
            bool weak = is_weak_.load(std::memory_order_acquire);
            if (weak) {
                return nullptr;
            }
            return ctrl->get_ptr();
        }

        T& operator*() const noexcept {
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);
            bool weak = is_weak_.load(std::memory_order_acquire);
            assert(!weak && ctrl && "Dereferencing null");
            return *ctrl->get_ptr();
        }

        T* operator->() const noexcept {
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);
            bool weak = is_weak_.load(std::memory_order_acquire);
            assert(!weak && ctrl && "Accessing through null");
            return ctrl->get_ptr();
        }

        explicit operator bool() const noexcept {
            bool weak = is_weak_.load(std::memory_order_acquire);
            if (weak) {
                return !expired();
            }
            return get() != nullptr;
        }

        size_t ref_count() const noexcept {
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);
            return ctrl ? ctrl->strong_count() : 0;
        }

        size_t weak_count() const noexcept {
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);
            return ctrl ? ctrl->weak_count() : 0;
        }

        bool unique() const noexcept {
            return ref_count() == 1;
        }

        bool is_weak() const noexcept {
            return is_weak_.load(std::memory_order_acquire);
        }

        void reset() noexcept {
            release();
            ctrl_.store(nullptr, std::memory_order_release);
            is_weak_.store(false, std::memory_order_release);
        }

        void reset(T* ptr) {
            Ptr tmp(ptr);
            swap(tmp);
        }

        void swap(Ptr& other) noexcept {
            ControlBlock<T>* my_ctrl = ctrl_.exchange(
                other.ctrl_.load(std::memory_order_acquire),
                std::memory_order_acq_rel);
            other.ctrl_.store(my_ctrl, std::memory_order_release);

            bool my_weak = is_weak_.exchange(
                other.is_weak_.load(std::memory_order_acquire),
                std::memory_order_acq_rel);
            other.is_weak_.store(my_weak, std::memory_order_release);
        }

        bool operator==(const Ptr& other) const noexcept {
            return get() == other.get();
        }

        bool operator!=(const Ptr& other) const noexcept {
            return !(*this == other);
        }

        bool operator==(std::nullptr_t) const noexcept {
            return get() == nullptr;
        }

        bool operator!=(std::nullptr_t) const noexcept {
            return get() != nullptr;
        }

    private:
        void release() noexcept {
            ControlBlock<T>* ctrl = ctrl_.load(std::memory_order_acquire);
            if (ctrl) {
                bool weak = is_weak_.load(std::memory_order_acquire);
                if (weak) {
                    ctrl->release_weak();
                }
                else {
                    ctrl->release_strong();
                }
            }
        }
    };

    template<typename T, typename... Args>
    Ptr<T> New(Args&&... args) {
        return Ptr<T>(new T(std::forward<Args>(args)...));
    }

#define GC_REF(ptr, member, value) (ptr)->member.Ref(value)

}

//---------------------------------------------------------------------------------------------

