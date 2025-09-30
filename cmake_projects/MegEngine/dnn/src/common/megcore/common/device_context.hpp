#pragma once

#include "megcore.h"

#include <memory>

namespace megcore {

class DeviceContext {
    public:
        static std::unique_ptr<DeviceContext> make(megcorePlatform_t platform,
                int deviceID, unsigned int flags);

        virtual ~DeviceContext() noexcept;

        megcorePlatform_t platform() const noexcept {
            return platform_;
        }

        int device_id() const noexcept {
            return device_id_;
        }

        unsigned int flags() const noexcept {
            return flags_;
        }

        virtual size_t mem_alignment_in_bytes() const noexcept = 0;

        virtual void activate() = 0;
        virtual void deactivate() {}
        virtual void *malloc(size_t size_in_bytes) = 0;
        virtual void free(void *ptr) = 0;

    protected:
        DeviceContext(megcorePlatform_t platform,
                int device_id, unsigned int flags):
            platform_(platform),
            device_id_(device_id),
            flags_(flags)
        {
        }

    private:
        megcorePlatform_t platform_;
        int device_id_;
        unsigned int flags_;
};

} // namespace megcore

// vim: syntax=cpp.doxygen
