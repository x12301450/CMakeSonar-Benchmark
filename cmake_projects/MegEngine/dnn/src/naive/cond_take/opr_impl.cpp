#include "./opr_impl.h"
#include "src/common/cond_take/predicate.cuh"
#include "src/common/utils.h"
#include "src/naive/handle.h"

using namespace megdnn;
using namespace naive;
using namespace cond_take;

using Param = CondTake::Param;

namespace {

template <uint32_t mode, typename ctype>
void gen_index(
        size_t sz, dt_int32* dest, const TensorND& mask,
        cond_take::Pred<mode, ctype> pred) {
    int didx = 0;
    auto inp = mask.ptr<ctype>();
    for (size_t i = 0; i < sz; ++i) {
        if (pred(inp[i])) {
            dest[didx++] = i;
        }
    }
    dest[sz] = didx;
}

template <typename ctype>
void copy_data(
        size_t sz, dt_int32* dest_idx, ctype* dest_data, const dt_int32* src_idx,
        const ctype* src_data) {
    for (size_t i = 0; i < sz; ++i) {
        auto idx = src_idx[i];
        dest_idx[i] = idx;
        dest_data[i] = src_data[idx];
    }
}

}  // anonymous namespace

size_t CondTakeImpl::get_workspace_in_bytes(
        const TensorLayout& data, const TensorLayout&) {
    return (data.total_nr_elems() + 1) * sizeof(dt_int32);
}

CondTakeImpl::Output CondTakeImpl::exec(
        _megdnn_tensor_in data, _megdnn_tensor_in mask, _megdnn_workspace workspace,
        DynOutMallocPolicyCall malloc_policy) {
#if !MGE_BUILD_WITHOUT_NAIVE_EXEC
    auto size = check_exec_get_size(data.layout, mask.layout, workspace.size);
    auto idx_tmp = workspace.ptr<dt_int32>();

    switch (mask.layout.dtype.enumv()) {
#define cb(_dt)                                      \
    case DTypeTrait<_dt>::enumv: {                   \
        using ctype = DTypeTrait<_dt>::ctype;        \
        dispatch_genidx<ctype>(size, idx_tmp, mask); \
        break;                                       \
    }
        MEGDNN_FOREACH_COMPUTING_DTYPE(cb)
        cb(::megdnn::dtype::Bool)
#undef cb
                default : megdnn_throw("bad mask dtype");
    }

    static_cast<HandleImpl*>(handle())->megcore_dispatcher()->sync();
    size_t out_size = idx_tmp[size];
    auto out_data = malloc_policy.alloc_output(0, data.layout.dtype, {out_size});
    auto out_idx = malloc_policy.alloc_output(1, dtype::Int32(), {out_size});

    switch (data.layout.dtype.enumv()) {
#define cb(_dt)                                                                    \
    case DTypeTrait<_dt>::enumv: {                                                 \
        using ctype = DTypeTrait<_dt>::ctype;                                      \
        MEGDNN_DISPATCH_CPU_KERN_OPR(copy_data<ctype>(                             \
                out_size, out_idx.ptr<dt_int32>(), out_data.ptr<ctype>(), idx_tmp, \
                data.ptr<ctype>()));                                               \
        break;                                                                     \
    }
        MEGDNN_FOREACH_COMPUTING_DTYPE(cb)
        cb(::megdnn::dtype::Bool)
#undef cb
                default : megdnn_throw("bad data dtype");
    }

    return {{out_data, out_idx}};
#else
    __builtin_trap();
    return {};
#endif
}

template <typename ctype>
void CondTakeImpl::dispatch_genidx(size_t size, dt_int32* dest, const TensorND& mask) {
    KParam kparam(m_param);
    switch (m_param.mode) {
#define cb(_m)                                                           \
    case Param::Mode::_m: {                                              \
        Pred<PEnum::_m, ctype> pred(kparam);                             \
        MEGDNN_DISPATCH_CPU_KERN_OPR(gen_index(size, dest, mask, pred)); \
        return;                                                          \
    }
        MEGDNN_FOREACH_COND_TAKE_MODE(cb)
#undef cb
    }
    megdnn_assert_internal(0);
}

// vim: syntax=cpp.doxygen
