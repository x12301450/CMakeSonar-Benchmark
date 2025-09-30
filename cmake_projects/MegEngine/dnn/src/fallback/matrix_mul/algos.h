#pragma once

#include <type_traits>
#include "src/common/algo_base.h"
#include "src/fallback/general_intrinsic/gi_common.h"
#include "src/fallback/matrix_mul/gemm_common.h"
#include "src/fallback/matrix_mul/opr_impl.h"

namespace megdnn {
namespace fallback {

class MatrixMulImpl::AlgoF32K8x12x1 final : public AlgoBase {
public:
    const char* name() const override { return "FB_F32_K8X12X1"; }
    bool usable(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override;
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::NAIVE;
    }
    MEGDNN_DECL_ALGO_TYPE(FB_F32K8x12x1)
    MEGDNN_REG_GEMM_FUNC_FOR_IM2COL();
};

class MatrixMulImpl::AlgoGemv final : public AlgoBase {
public:
    const char* name() const override { return "FB_GEMV"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::NAIVE;
    }
    MEGDNN_DECL_ALGO_TYPE(FB_GEMV)
    MEGDNN_OVERRIDE_MATMUL_DESC(
            8, 16, 1, 4,
            static_cast<AlgoDataType>(
                    static_cast<uint32_t>(AlgoDataType::FLOAT16) |
                    static_cast<uint32_t>(AlgoDataType::FLOAT32) |
                    static_cast<uint32_t>(AlgoDataType::INT8X8X16) |
                    static_cast<uint32_t>(AlgoDataType::QINT8X8X32) |
                    static_cast<uint32_t>(AlgoDataType::QUINT8X8X32)),
            DEFAULT)
};

class MatrixMulImpl::AlgoNaive final : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::NAIVE;
    }
    const char* name() const override { return "FB_NAIVE"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override;
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMM; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_DECL_ALGO_TYPE(FB_NAIVE)
    MEGDNN_OVERRIDE_MATMUL_DESC(
            8, 16, 1, 4,
            static_cast<AlgoDataType>(
                    static_cast<uint32_t>(AlgoDataType::FLOAT16) |
                    static_cast<uint32_t>(AlgoDataType::FLOAT32) |
                    static_cast<uint32_t>(AlgoDataType::INT8X8X16) |
                    static_cast<uint32_t>(AlgoDataType::QINT8X8X32) |
                    static_cast<uint32_t>(AlgoDataType::QUINT8X8X32)),
            DEFAULT)
};

class MatrixMulImpl::AlgoF32GiGemvMK4 : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::USABLE_DEPEND_ON_SHAPE;
    }
    const char* name() const override { return "FB_GI_F32_GEMV_MK4"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(4, 1, 1, 4, AlgoDataType::FLOAT32, MK4)
    MEGDNN_DECL_ALGO_TYPE(FB_GI_F32_GEMV_MK4)
};

class MatrixMulImpl::AlgoF32GiMK4_4x8 final : public AlgoBase {
public:
    AlgoAttribute attribute() const override { return AlgoAttribute::REPRODUCIBLE; }
    const char* name() const override { return "FB_GI_F32_MK4_4x8"; }
    bool usable(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override;
    kern_t get_kern(const KernSizeParam&) const override;
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(4, 8, 4, 4, AlgoDataType::FLOAT32, MK4)
    MEGDNN_DECL_ALGO_TYPE(FB_GI_F32_MK4_4x8)
};

#if defined(GI_SUPPORT_F16)
class MatrixMulImpl::AlgoF16GiMK8_8x8 final : public AlgoBase {
public:
    AlgoAttribute attribute() const override { return AlgoAttribute::REPRODUCIBLE; }
    const char* name() const override { return "FB_GI_F16_MK8_8x8"; }
    bool usable(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override;
    kern_t get_kern(const KernSizeParam&) const override;
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(8, 8, 8, 8, AlgoDataType::FLOAT16, MK8)
    MEGDNN_DECL_ALGO_TYPE(FB_GI_F16_MK8_8x8)
};
#endif

class MatrixMulImpl::AlgoF32GiMK4Pack4x12 final : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::USABLE_DEPEND_ON_SHAPE;
    }
    const char* name() const override { return "FB_GI_F32_MK4_PACK_4x12"; }
    bool usable(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override;
    kern_t get_kern(const KernSizeParam&) const override;
    MEGDNN_REG_GEMM_FUNC_FOR_IM2COL();
    MEGDNN_DECL_ALGO_TYPE(FB_GI_F32_MK4_PACK_4x12)
};

class MatrixMulImpl::AlgoF32Gi4x12 final : public AlgoBase {
public:
    AlgoAttribute attribute() const override { return AlgoAttribute::REPRODUCIBLE; }
    const char* name() const override { return "FB_GI_F32_4x12"; }
    bool usable(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override;
    kern_t get_kern(const KernSizeParam&) const override;
    MEGDNN_REG_GEMM_FUNC_FOR_IM2COL();
    MEGDNN_DECL_ALGO_TYPE(FB_GI_F32_4x12)
};

}  // namespace fallback
}  // namespace megdnn

// vim: syntax=cpp.doxygen
