#pragma once

#include "./device_context.hpp"

namespace megcore {

class ComputingContext {
    public:
        static std::unique_ptr<ComputingContext> make(
                megcoreDeviceHandle_t dev_handle, unsigned int flags);

        virtual ~ComputingContext() noexcept;

        megcoreDeviceHandle_t dev_handle() const noexcept {
            return dev_handle_;
        }

        unsigned int flags() const noexcept {
            return flags_;
        }

        virtual void memcpy(void *dst, const void *src,
                size_t size_in_bytes,
                megcoreMemcpyKind_t kind) = 0;
        virtual void memset(void *dst, int value, size_t size_in_bytes) = 0;
        virtual void synchronize() = 0;

    protected:
        ComputingContext(megcoreDeviceHandle_t dev_handle, unsigned int flags):
            dev_handle_{dev_handle},
            flags_{flags}
        {}

    private:
        megcoreDeviceHandle_t dev_handle_;
        unsigned int flags_;
};

} // namespace megcore

// vim: syntax=cpp.doxygen
