#pragma once

#include "src/arm_common/matrix_mul/opr_impl.h"
#include "src/fallback/matrix_mul/gemm_common.h"

namespace megdnn {
namespace arm_common {

class MatrixMulImpl::AlgoInt8x8x16 final : public AlgoBase {
public:
    AlgoAttribute attribute() const override { return AlgoAttribute::REPRODUCIBLE; }
    const char* name() const override { return "ARM_COMMON_INT8X8X16"; }
    bool usable(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override;
    kern_t get_kern(const KernSizeParam&) const override;
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(8, 16, 1, 4, AlgoDataType::INT8X8X16, DEFAULT)
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_INT8X8X16)
};

class MatrixMulImpl::AlgoInt8x8x32Gemv : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::USABLE_DEPEND_ON_SHAPE;
    }
    const char* name() const override { return "ARM_COMMON_INT8X8X32_GEMV"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(8, 16, 1, 2, AlgoDataType::QINT8X8X32, DEFAULT)
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_INT8X8X32_GEMV)
};

class MatrixMulImpl::AlgoInt8x8x32GemvMK4 : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::USABLE_DEPEND_ON_SHAPE;
    }
    const char* name() const override { return "ARM_COMMON_INT8X8X32_GEMV_MK4"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(8, 16, 1, 2, AlgoDataType::QINT8X8X32, MK4)
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_INT8X8X32_GEMV_MK4)
};
#if MGB_ENABLE_DOT
class MatrixMulImpl::AlgoInt8x8x32GevmDot : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::USABLE_DEPEND_ON_SHAPE;
    }
    const char* name() const override { return "ARM_COMMON_INT8X8X32_GEVM_DOT"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEVM; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(1, 32, 4, 2, AlgoDataType::QINT8X8X32, DEFAULT)
    WorkspaceBundle get_bundle(const KernSizeParam&) const override {
        return WorkspaceBundle{nullptr, {}};
    }
    kern_naked_t get_kern_naked(const KernSizeParam&) const override {
        megdnn_assert(0, "naked kern no impl");
    }
    void pack_A(const KernParam& kern_param, void* out, size_t index, size_t stride)
            const override {
        megdnn_assert(0, "pack_A no impl");
    }
    void pack_B(const KernParam& kern_param, void* out, size_t x0, size_t xmax)
            const override {
        megdnn_assert(0, "pack_B no impl");
    }
    InnerBlockSize get_inner_block_size() const override { return {1, 32, 4}; };
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_INT8X8X32_GEVM_DOT)
};

class MatrixMulImpl::AlgoInt8x8x32GevmN32K4Dot : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::USABLE_DEPEND_ON_SHAPE;
    }
    const char* name() const override { return "ARM_COMMON_INT8X8X32_GEVM_N32K4_DOT"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEVM; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(1, 32, 4, 2, AlgoDataType::QINT8X8X32, N32K4_DOT)
    WorkspaceBundle get_bundle(const KernSizeParam&) const override {
        return WorkspaceBundle{nullptr, {}};
    }
    kern_naked_t get_kern_naked(const KernSizeParam&) const override {
        megdnn_assert(0, "naked kern no impl");
    }
    void pack_A(const KernParam& kern_param, void* out, size_t index, size_t stride)
            const override {
        megdnn_assert(0, "pack_A no impl");
    }
    void pack_B(const KernParam& kern_param, void* out, size_t x0, size_t xmax)
            const override {
        megdnn_assert(0, "pack_B no impl");
    }
    InnerBlockSize get_inner_block_size() const override { return {1, 32, 4}; };
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_INT8X8X32_GEVM_N32K4_DOT)
};

class MatrixMulImpl::AlgoInt8x8x32GemvMK4Dot : public AlgoBase {
public:
    AlgoAttribute attribute() const override {
        return AlgoAttribute::REPRODUCIBLE | AlgoAttribute::USABLE_DEPEND_ON_SHAPE;
    }
    const char* name() const override { return "ARM_COMMON_INT8X8X32_GEMV_MK4_DOT"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(8, 16, 1, 2, AlgoDataType::QINT8X8X32, MK4_DOT)
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_INT8X8X32_GEMV_MK4_DOT)
};
#endif

class MatrixMulImpl::AlgoF32Gemv : public AlgoBase {
protected:
    ~AlgoF32Gemv() = default;

public:
    AlgoAttribute attribute() const override { return AlgoAttribute::REPRODUCIBLE; }
    const char* name() const override { return "ARM_COMMON_F32_GEMV"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(8, 16, 1, 4, AlgoDataType::FLOAT32, DEFAULT)
};

#if __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
class MatrixMulImpl::AlgoF16Gemv : public AlgoBase {
public:
    AlgoAttribute attribute() const override { return AlgoAttribute::REPRODUCIBLE; }
    const char* name() const override { return "ARM_COMMON_F16_GEMV"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(8, 16, 1, 2, AlgoDataType::FLOAT16, DEFAULT)
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_F16_GEMV)
};
#endif

class MatrixMulImpl::AlgoGevm : public AlgoBase {
public:
    AlgoAttribute attribute() const override { return AlgoAttribute::REPRODUCIBLE; }
    const char* name() const override { return "ARM_COMMON_GEVM"; }
    bool usable(const KernSizeParam&) const override;
    bool preferred(const KernSizeParam&) const override;
    size_t get_workspace(const KernSizeParam&) const override { return 0; }
    kern_t get_kern(const KernSizeParam&) const override;
    AlgoSet algoset() const override { return AlgoSet::ALGO_TYPE_GEMV; }
    PackMode packmode() const override { return PackMode::NO_PACK; }
    MEGDNN_OVERRIDE_MATMUL_DESC(
            1, 1, 1, 4,
            static_cast<AlgoDataType>(
                    static_cast<uint32_t>(AlgoDataType::FLOAT16) |
                    static_cast<uint32_t>(AlgoDataType::FLOAT32) |
                    static_cast<uint32_t>(AlgoDataType::QINT8X8X32)),
            DEFAULT)
    MEGDNN_DECL_ALGO_TYPE(ARM_COMMON_GEVM)
};

}  // namespace arm_common
}  // namespace megdnn

// vim: syntax=cpp.doxygen
