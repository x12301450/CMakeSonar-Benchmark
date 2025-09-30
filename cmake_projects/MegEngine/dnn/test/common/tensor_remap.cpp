#include "test/common/tensor_remap.h"

#include <cstring>
#include "test/common/random_state.h"

namespace megdnn {
namespace test {
namespace tensor_remap {

dt_float32 MapRNG::gen_single_val() {
    auto&& gen = RandomState::generator();
    std::uniform_int_distribution<int> dist(0, m_src[m_cnt] - 1);
    m_cnt++;
    if (m_cnt == m_src.ndim)
        m_cnt -= m_src.ndim;
    return dist(gen);
}

NonoverlappingMapRNG::NonoverlappingMapRNG(TensorShape src)
        : m_cnt(0), m_src(src), m_idx(TensorLayout(src, dtype::Byte()), 0) {}

dt_float32 NonoverlappingMapRNG::gen_single_val() {
    auto res = m_idx.array()[m_cnt];
    m_cnt++;
    if (m_cnt == m_src.ndim) {
        m_cnt -= m_src.ndim;
        m_idx = Index(m_idx.layout(), m_idx.linear_index() + 1);
    }
    return res;
}

}  // namespace tensor_remap
}  // namespace test
}  // namespace megdnn

// vim: syntax=cpp.doxygen
