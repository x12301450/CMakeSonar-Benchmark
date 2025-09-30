#ifndef DEF_TRAIT
#error "DEF_TRAIT must be defined"
#endif

/* ======================= binary ======================= */
#define _CUR_ARITY 2
#define _EXPAND_PARAMS     \
    ctype x = inp[0][idx]; \
    ctype y = inp[1][idx]

#define _ALLOW_BOOL  true
#define _ALLOW_FLOAT false
#define _ALLOW_INT   false
DEF_TRAIT(AND, x&& y)
DEF_TRAIT(OR, x || y)
DEF_TRAIT(XOR, x ^ y)
#undef _ALLOW_INT
#undef _ALLOW_FLOAT

#define _ALLOW_INT   true
#define _ALLOW_FLOAT true
DEF_TRAIT(EQ, x == y)
DEF_TRAIT(LEQ, x <= y)
DEF_TRAIT(LT, x < y)

#undef _ALLOW_BOOL

#define _ALLOW_BOOL  false
#define _ALLOW_FLOAT true
#define _ALLOW_INT   true
DEF_TRAIT(ABS_GRAD, x > 0 ? y : -y)
DEF_TRAIT(ADD, x + y)
DEF_TRAIT(FLOOR_DIV, do_floor_div(x, y))
DEF_TRAIT(MAX, std::max(x, y))
DEF_TRAIT(MIN, std::min(x, y))
DEF_TRAIT(MOD, do_mod(x, y))
DEF_TRAIT(MUL, x* y)
DEF_TRAIT(SIGMOID_GRAD, x*(1 - x) * y)
DEF_TRAIT(SUB, x - y)
DEF_TRAIT(SWITCH_GT0, x > 0 ? y : 0)
DEF_TRAIT(TANH_GRAD, (1 - x * x) * y)

DEF_TRAIT(FUSE_ADD_RELU, std::max<ctype>(x + y, 0))
DEF_TRAIT(PRELU, (x > 0) ? x : (x* y))
#undef _ALLOW_INT

#define _ALLOW_INT false
DEF_TRAIT(POW, std::pow(x, y))
DEF_TRAIT(TRUE_DIV, x / y)
DEF_TRAIT(LOG_SUM_EXP, do_log_sum_exp(x, y))
DEF_TRAIT(FUSE_ADD_SIGMOID, 1 / (1 + std::exp(-(x + y))))
DEF_TRAIT(FUSE_ADD_TANH, std::tanh(x + y))
DEF_TRAIT(FUSE_ADD_H_SWISH, do_fuse_add_h_swish(x, y))
DEF_TRAIT(FAST_TANH_GRAD, do_fast_tanh_grad(x, y))
DEF_TRAIT(ATAN2, std::atan2(x, y))
DEF_TRAIT(H_SWISH_GRAD, do_h_swish_grad(x, y))
DEF_TRAIT(
        SILU_GRAD, y*(1 + std::exp(-x) + x * std::exp(-x)) / (1 + std::exp(-x)) /
                           (1 + std::exp(-x)))
DEF_TRAIT(GELU_GRAD, do_gelu_grad(x, y))
DEF_TRAIT(ASINH_GRAD, y / std::sqrt(x * x + 1))
DEF_TRAIT(ACOSH_GRAD, y / std::sqrt(x * x - 1))
DEF_TRAIT(ATANH_GRAD, y / (1 - x * x))
DEF_TRAIT(SOFTPLUS_GRAD, do_softplus_grad(x, y))
DEF_TRAIT(RELU6_GRAD, x <= 0.f ? 0.f : (x >= 6.f ? 0.f : y))
DEF_TRAIT(HSIGMOID_GRAD, x <= -3.f ? 0.f : (x >= 3.f ? 0.f : (y / 6.f)))

#undef _ALLOW_INT
#undef _ALLOW_FLOAT

#define _ALLOW_FLOAT false
#define _ALLOW_INT   true
DEF_TRAIT(SHL, do_shl(x, y))
DEF_TRAIT(SHR, do_shr(x, y))
DEF_TRAIT(RMULH, do_round_mulh_saturate(x, y))
#undef _ALLOW_INT
#undef _ALLOW_FLOAT
#undef _ALLOW_BOOL

#undef _CUR_ARITY
#undef _EXPAND_PARAMS

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
