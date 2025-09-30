#pragma once

#include "../impl_helper.h"

namespace mgb {
class ROCmCompNode final : public CompNodeImplHelper {
public:
    static constexpr Flag sm_flag =
            Flag::QUEUE_LIMITED | Flag::HAS_COPY_STREAM | Flag::SUPPORT_UNIFIED_ADDRESS;

    class CompNodeImpl;
    class EventImpl;

    //! whether rocm comp node is available
    static bool available();

    static void try_coalesce_all_free_memory();
    static void foreach (thin_function<void(CompNode)> callback);
    static void finalize();
    static size_t get_device_count();
    static Impl* load_rocm(const Locator& locator, const Locator& locator_logical);
    static void sync_all();
};
}  // namespace mgb

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
