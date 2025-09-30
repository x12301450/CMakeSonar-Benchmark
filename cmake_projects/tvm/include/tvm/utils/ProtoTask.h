/** Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#pragma once

#include <tvm/api.h>

#include <tvm/utils/AffineExpr.h>
#include <tvm/utils/internal/ProtoTaskDetails.h>

namespace tvm
{

namespace utils
{

template<constraint::Type T, typename FunT>
class ProtoTaskCommon;

/** A utility class to represent the "constraint" part of a Task, for general functions*/
template<constraint::Type T>
using ProtoTask = ProtoTaskCommon<T, FunctionPtr>;

/** A utility class to represent the "constraint" part of a Task, specialized for linear functions*/
template<constraint::Type T>
using LinearProtoTask = ProtoTaskCommon<T, LinearFunctionPtr>;

template<constraint::Type T, typename FunT>
class ProtoTaskCommon
{
public:
  ProtoTaskCommon(FunT f, const internal::RHS & rhs) : f_(f), rhs_(rhs)
  {
    if(rhs.type_ == internal::RHSType::Vector && f->rSize() != rhs.v_.size())
    {
      throw std::runtime_error("The vector you provided has not the correct size.");
    }
  }

  template<typename FunU>
  ProtoTaskCommon(const ProtoTaskCommon<T, FunU> & pt) : f_(pt.f_), rhs_(pt.rhs_)
  {}

  FunT f_;
  internal::RHS rhs_;
};

template<typename FunT>
class ProtoTaskCommon<constraint::Type::DOUBLE_SIDED, FunT>
{
public:
  ProtoTaskCommon(FunT f, const internal::RHS & l, const internal::RHS & u) : f_(f), l_(l), u_(u)
  {
    if(l.type_ == internal::RHSType::Vector && f->rSize() != l.v_.size())
    {
      throw std::runtime_error("The lower bound vector you provided has not the correct size.");
    }
    if(u.type_ == internal::RHSType::Vector && f->rSize() != u.v_.size())
    {
      throw std::runtime_error("The upper bound vector you provided has not the correct size.");
    }
  }

  template<typename FunU>
  ProtoTaskCommon(const ProtoTaskCommon<constraint::Type::DOUBLE_SIDED, FunU> & pt) : f_(pt.f_), l_(pt.l_), u_(pt.u_)
  {}

  FunctionPtr f_;
  internal::RHS l_;
  internal::RHS u_;
};

/** A helper alias that is IfNotLinearFunction if T is a tvm::abstract::Function,
 * but not a tvm::abstract::LinearFunction, IfLinearFunction if it is a
 * tvm::abstract::LinearFunction, and nothing (discarded by SFINAE) otherwise.
 */
template<typename T, typename IfNotLinearFunction, typename IfLinearFunction>
using ProtoChoice =
    typename std::enable_if<std::is_base_of<tvm::function::abstract::Function, T>::value,
                            typename std::conditional<std::is_base_of<tvm::function::abstract::LinearFunction, T>::value,
                                                      IfLinearFunction,
                                                      IfNotLinearFunction>::type>::type;

/** Equality ProtoTask f = rhs*/
using ProtoTaskEQ = ProtoTask<constraint::Type::EQUAL>;
using LinearProtoTaskEQ = LinearProtoTask<constraint::Type::EQUAL>;
template<typename T>
using ProtoTaskEQRet = ProtoChoice<T, tvm::utils::ProtoTaskEQ, tvm::utils::LinearProtoTaskEQ>;

/** Inequality ProtoTask f <= rhs*/
using ProtoTaskLT = ProtoTask<constraint::Type::LOWER_THAN>;
using LinearProtoTaskLT = LinearProtoTask<constraint::Type::LOWER_THAN>;
template<typename T>
using ProtoTaskLTRet = ProtoChoice<T, tvm::utils::ProtoTaskLT, tvm::utils::LinearProtoTaskLT>;

/** Inequality ProtoTask f >= rhs*/
using ProtoTaskGT = ProtoTask<constraint::Type::GREATER_THAN>;
using LinearProtoTaskGT = LinearProtoTask<constraint::Type::GREATER_THAN>;
template<typename T>
using ProtoTaskGTRet = ProtoChoice<T, tvm::utils::ProtoTaskGT, tvm::utils::LinearProtoTaskGT>;

/** Double sided inequality ProtoTask l <= f <= u*/
using ProtoTaskDS = ProtoTask<constraint::Type::DOUBLE_SIDED>;
using LinearProtoTaskDS = LinearProtoTask<constraint::Type::DOUBLE_SIDED>;
template<typename T>
using ProtoTaskDSRet = ProtoChoice<T, tvm::utils::ProtoTaskDS, tvm::utils::LinearProtoTaskDS>;

} // namespace utils

} // namespace tvm

/** Convenience operators to form a ProtoTask or LinearProtoTask f op rhs
 * (or l <= f <= u)
 *
 * \param f the function to form the task
 * \param rhs a double or a Eigen::Vector with the same size as the function.
 * Note that for a double you need to explicitly write a double (e.g 0.,
 * not 0), otherwise the compiler won't be able to decide which overload to
 * pick between this and shared_ptr operator.
 */
/**@{*/
// clang-format off
template<typename F>
inline tvm::utils::ProtoTaskEQRet<F> operator==(std::shared_ptr<F> f, const tvm::utils::internal::RHS& rhs) { return { f, rhs }; }
template<typename F>
inline tvm::utils::ProtoTaskEQRet<F> operator==(const tvm::utils::internal::RHS& rhs, std::shared_ptr<F> f) { return { f, rhs }; }
template<typename F>
inline tvm::utils::ProtoTaskGTRet<F> operator>=(std::shared_ptr<F> f, const tvm::utils::internal::RHS& rhs) { return { f, rhs }; }
template<typename F>
inline tvm::utils::ProtoTaskLTRet<F> operator>=(const tvm::utils::internal::RHS& rhs, std::shared_ptr<F> f) { return { f, rhs }; }
template<typename F>
inline tvm::utils::ProtoTaskLTRet<F> operator<=(std::shared_ptr<F> f, const tvm::utils::internal::RHS& rhs) { return { f, rhs }; }
template<typename F>
inline tvm::utils::ProtoTaskGTRet<F> operator<=(const tvm::utils::internal::RHS& rhs, std::shared_ptr<F> f) { return { f, rhs }; }

inline tvm::utils::ProtoTaskDS operator>=(const tvm::utils::ProtoTaskLT& ptl, const tvm::utils::internal::RHS& rhs) { return { ptl.f_, rhs, ptl.rhs_ }; }
inline tvm::utils::ProtoTaskDS operator<=(const tvm::utils::ProtoTaskGT& ptg, const tvm::utils::internal::RHS& rhs) { return { ptg.f_, ptg.rhs_, rhs }; }
inline tvm::utils::LinearProtoTaskDS operator>=(const tvm::utils::LinearProtoTaskLT& ptl, const tvm::utils::internal::RHS& rhs) { return { ptl.f_, rhs, ptl.rhs_ }; }
inline tvm::utils::LinearProtoTaskDS operator<=(const tvm::utils::LinearProtoTaskGT& ptg, const tvm::utils::internal::RHS& rhs) { return { ptg.f_, ptg.rhs_, rhs }; }
// clang-format on
/**@}*/

#define TVM_ID(x) std::make_shared<tvm::function::IdentityFunction>(x)
/** Convenience operators to form a LinearProtoTask x op rhs (or l <= x <= u)
 *
 * \param x the variable used in the task
 * \param rhs a double or a Eigen::Vector with the same size as the function.
 * Note that for a double you need to explicitly write a double (e.g 0.,
 * not 0), otherwise the compiler won't be able to decide which overload to
 * pick between this and shared_ptr operator.
 */
/**@{*/
// clang-format off
inline tvm::utils::LinearProtoTaskEQ operator==(tvm::VariablePtr x, const tvm::utils::internal::RHS& rhs) { return TVM_ID(x) == rhs; }
inline tvm::utils::LinearProtoTaskEQ operator==(const tvm::utils::internal::RHS& rhs, tvm::VariablePtr x) { return TVM_ID(x) == rhs; }
inline tvm::utils::LinearProtoTaskGT operator>=(tvm::VariablePtr x, const tvm::utils::internal::RHS& rhs) { return TVM_ID(x) >= rhs; }
inline tvm::utils::LinearProtoTaskLT operator>=(const tvm::utils::internal::RHS& rhs, tvm::VariablePtr x) { return TVM_ID(x) <= rhs; }
inline tvm::utils::LinearProtoTaskLT operator<=(tvm::VariablePtr x, const tvm::utils::internal::RHS& rhs) { return TVM_ID(x) <= rhs; }
inline tvm::utils::LinearProtoTaskGT operator<=(const tvm::utils::internal::RHS& rhs, tvm::VariablePtr x) { return TVM_ID(x) >= rhs; }
// clang-format on
/**@}*/
#undef TVM_ID

#define TVM_LIN(x) std::make_shared<tvm::function::BasicLinearFunction>(x)

/** Convenience operators to form a LinearProtoTask expr op rhs (or l <= expr <= u)
 * where expr is a linear expression of the form matrixExpr * VariablePtr or an
 * affine expression as a sum of linear expressions and vectorExpr.
 *
 * \param lin the linear expression to form the task
 * \param aff the affine expression to form the task
 * \param rhs a double or a Eigen::Vector with the same size as the function.
 * Note that for a double you don't need to explicitly write a double (e.g 0.)
 * to the contrary of the operators working with shared_ptr on Function.
 */
///@{
// clang-format off
template<typename Derived>
inline tvm::utils::LinearProtoTaskEQ operator==(const tvm::utils::LinearExpr<Derived>& lin, const tvm::utils::internal::RHS& rhs) { return TVM_LIN(lin) == rhs; }
template<typename Derived>
inline tvm::utils::LinearProtoTaskEQ operator==(const tvm::utils::internal::RHS& rhs, const tvm::utils::LinearExpr<Derived>& lin) { return TVM_LIN(lin) == rhs; }
template<typename Derived>
inline tvm::utils::LinearProtoTaskGT operator>=(const tvm::utils::LinearExpr<Derived>& lin, const tvm::utils::internal::RHS& rhs) { return TVM_LIN(lin) >= rhs; }
template<typename Derived>
inline tvm::utils::LinearProtoTaskLT operator>=(const tvm::utils::internal::RHS& rhs, const tvm::utils::LinearExpr<Derived>& lin) { return TVM_LIN(lin) <= rhs; }
template<typename Derived>
inline tvm::utils::LinearProtoTaskLT operator<=(const tvm::utils::LinearExpr<Derived>& lin, const tvm::utils::internal::RHS& rhs) { return TVM_LIN(lin) <= rhs; }
template<typename Derived>
inline tvm::utils::LinearProtoTaskGT operator<=(const tvm::utils::internal::RHS& rhs, const tvm::utils::LinearExpr<Derived>& lin) { return TVM_LIN(lin) >= rhs; }

template<typename CstDerived, typename... Derived>
inline tvm::utils::LinearProtoTaskEQ operator==(const tvm::utils::AffineExpr<CstDerived, Derived...>& aff, const tvm::utils::internal::RHS& rhs) { return TVM_LIN(aff) == rhs; }
template<typename CstDerived, typename... Derived>
inline tvm::utils::LinearProtoTaskEQ operator==(const tvm::utils::internal::RHS& rhs, const tvm::utils::AffineExpr<CstDerived, Derived...>& aff) { return TVM_LIN(aff) == rhs; }
template<typename CstDerived, typename... Derived>
inline tvm::utils::LinearProtoTaskGT operator>=(const tvm::utils::AffineExpr<CstDerived, Derived...>& aff, const tvm::utils::internal::RHS& rhs) { return TVM_LIN(aff) >= rhs; }
template<typename CstDerived, typename... Derived>
inline tvm::utils::LinearProtoTaskLT operator>=(const tvm::utils::internal::RHS& rhs, const tvm::utils::AffineExpr<CstDerived, Derived...>& aff) { return TVM_LIN(aff) <= rhs; }
template<typename CstDerived, typename... Derived>
inline tvm::utils::LinearProtoTaskLT operator<=(const tvm::utils::AffineExpr<CstDerived, Derived...>& aff, const tvm::utils::internal::RHS& rhs) { return TVM_LIN(aff) <= rhs; }
template<typename CstDerived, typename... Derived>
inline tvm::utils::LinearProtoTaskGT operator<=(const tvm::utils::internal::RHS& rhs, const tvm::utils::AffineExpr<CstDerived, Derived...>& aff) { return TVM_LIN(aff) >= rhs; }
// clang-format on
///@}
#undef TVM_LIN
