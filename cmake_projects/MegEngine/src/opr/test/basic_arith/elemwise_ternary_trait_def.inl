#ifndef DEF_TRAIT
#error "DEF_TRAIT must be defined"
#endif

/* ======================= ternary ======================= */
#define _CUR_ARITY 3
#define _EXPAND_PARAMS     \
    ctype x = inp[0][idx]; \
    ctype y = inp[1][idx]; \
    ctype z = inp[2][idx]

#define _ALLOW_BOOL  false
#define _ALLOW_FLOAT true
#define _ALLOW_INT   true
DEF_TRAIT(COND_LEQ_MOV, x <= y ? z : 0)
DEF_TRAIT(COND_LT_MOV, x < y ? z : 0)
DEF_TRAIT(FUSE_MUL_ADD3, x* y + z)
DEF_TRAIT(CLIP, x < y ? y : (x < z ? x : z))
#undef _ALLOW_INT
#define _ALLOW_INT false
DEF_TRAIT(PRELU_GRAD, x > 0 ? y : (y * z))
#undef _ALLOW_INT
#undef _ALLOW_FLOAT

#undef _CUR_ARITY
#undef _EXPAND_PARAMS

/* ======================= quaternary ======================= */
#define _CUR_ARITY 4
#define _EXPAND_PARAMS      \
    ctype i0 = inp[0][idx]; \
    ctype i1 = inp[1][idx]; \
    ctype i2 = inp[2][idx]; \
    ctype i3 = inp[3][idx]

#define _ALLOW_FLOAT true
#define _ALLOW_INT   true
DEF_TRAIT(FUSE_MUL_ADD4, i0* i1 + i2 * i3)
#undef _ALLOW_INT
#undef _ALLOW_FLOAT

#undef _CUR_ARITY
#undef _EXPAND_PARAMS
#undef _ALLOW_BOOL

// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
