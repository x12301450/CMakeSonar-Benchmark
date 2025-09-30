#pragma once
#include "src/common/utils.h"
#include "src/naive/handle.h"

#include <mutex>

namespace megdnn {
namespace fallback {

class HandleImpl : public naive::HandleImpl {
public:
    HandleImpl(
            megcoreComputingHandle_t computing_handle,
            HandleType type = HandleType::FALLBACK)
            : naive::HandleImpl::HandleImpl(computing_handle, type) {}

    template <typename Opr>
    std::unique_ptr<Opr> create_operator();

    //! global relayout opr
    Relayout* relayout_opr() override final {
        return get_helper_opr<Relayout, 3>(this);
    }
};

}  // namespace fallback
}  // namespace megdnn

// vim: syntax=cpp.doxygen
