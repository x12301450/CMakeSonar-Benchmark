/** Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#pragma once

#include <tvm/function/abstract/LinearFunction.h>
#include <tvm/internal/MatrixProperties.h>
#include <tvm/internal/VariableCountingVector.h>
#include <tvm/utils/AffineExpr.h>

#include <utility>

namespace tvm
{

namespace function
{

/** The most basic linear function f(x_1, ..., x_k) = sum A_i x_i + b
 * where the matrices are constant.
 */
class TVM_DLLAPI BasicLinearFunction : public abstract::LinearFunction
{
public:
  /** A x (b = 0) */
  BasicLinearFunction(const MatrixConstRef & A, VariablePtr x);
  /** A1 x1 + ... An xn (b = 0) */
  BasicLinearFunction(const std::vector<MatrixConstRef> & A, const std::vector<VariablePtr> & x);

  /** A x + b */
  BasicLinearFunction(const MatrixConstRef & A, VariablePtr x, const VectorConstRef & b);
  /** A1 x1 + ... An xn + b*/
  BasicLinearFunction(const std::vector<MatrixConstRef> & A,
                      const std::vector<VariablePtr> & x,
                      const VectorConstRef & b);

  /** Uninitialized version for a function of size \p m with a single variable
   * \p x
   * Don't forget to initialize A \b and b
   */
  BasicLinearFunction(int m, VariablePtr x);

  /** Uninitialized version for a function of size \p m with multiple
   * variables \p x1 ... \p xn
   * Don't forget to initialize the Ai \b and b
   */
  BasicLinearFunction(int m, const std::vector<VariablePtr> & x);

  /** Initialization from a utils::LinearExpr. Meant to be used by operators in
   * ProtoTasks.h
   */
  template<typename Derived>
  BasicLinearFunction(const utils::LinearExpr<Derived> & lin);

  /** Initialization from a utils::AffineExpr. Meant to be used by operators in
   * ProtoTasks.h
   */
  template<typename CstDerived, typename... Derived>
  BasicLinearFunction(const utils::AffineExpr<CstDerived, Derived...> & aff);

  /** Set the matrix \p A corresponding to variable \p x and optionally the
   * properties \p p of \p A.*/
  virtual void A(const MatrixConstRef & A, const Variable & x, const tvm::internal::MatrixProperties & p = {});
  /** Shortcut for when there is a single variable.*/
  virtual void A(const MatrixConstRef & A, const tvm::internal::MatrixProperties & p = {});
  /** Set the constant term \p b, and optionally its properties \p p.*/
  virtual void b(const VectorConstRef & b, const tvm::internal::MatrixProperties & p = {});

  using LinearFunction::b;

private:
  template<typename Derived>
  void add(const Eigen::MatrixBase<Derived> & A, VariablePtr x);

  template<typename Tuple, size_t... Indices>
  void add(Tuple && tuple, std::index_sequence<Indices...>);

  template<typename Derived>
  void add(const utils::LinearExpr<Derived> & lin);
};

namespace internal
{
template<typename Tuple, size_t... Indices>
void addVar(tvm::internal::VariableCountingVector & v, Tuple && tuple, std::index_sequence<Indices...>)
{
  (v.add(std::get<Indices>(std::forward<Tuple>(tuple)).variable()), ...);
}
} // namespace internal

template<typename Derived>
BasicLinearFunction::BasicLinearFunction(const utils::LinearExpr<Derived> & lin)
: LinearFunction(static_cast<int>(lin.matrix().rows()))
{
  add(lin);
  this->b(Eigen::VectorXd::Zero(size()),
          {tvm::internal::MatrixProperties::Constness(true), tvm::internal::MatrixProperties::ZERO});
  setDerivativesToZero();
}

template<typename CstDerived, typename... Derived>
BasicLinearFunction::BasicLinearFunction(const utils::AffineExpr<CstDerived, Derived...> & aff)
: LinearFunction(static_cast<int>(std::get<0>(aff.linear()).matrix().rows()))
{
  using Indices = std::make_index_sequence<sizeof...(Derived)>;
  tvm::internal::VariableCountingVector v;
  internal::addVar(v, aff.linear(), Indices{});
  const auto & vars = v.variables();
  const auto & simple = v.simple(); // simple[i]==true means vars[i] that appears in v based on a single first add to v
  for(int i = 0; i < vars.numberOfVariables(); ++i)
  {
    if(!simple[i]) // if vars[i] is the results of adding several potentially overlapping variables to v...
    {
      addVariable(vars[i], true); // ... we add the variable now and initialize its jacobian to 0 below.
                                  // This will force using += to fill the part of the jacobian corresponding to
                                  // a subvariable in method add. Simple variables will be added directly in add.
    }
  }
  for(auto & J : jacobian_)
  {
    J.second.setZero();
    J.second.properties({tvm::internal::MatrixProperties::Constness(true)});
  }
  add(aff.linear(), Indices{});
  if constexpr(std::is_same_v<CstDerived, utils::internal::NoConstant>)
  {
    this->b(Eigen::VectorXd::Zero(size()),
            {tvm::internal::MatrixProperties::Constness(true), tvm::internal::MatrixProperties::ZERO});
  }
  else
  {
    this->b(aff.constant(), {tvm::internal::MatrixProperties::Constness(true)});
  }
  setDerivativesToZero();
}

template<typename Derived>
inline void BasicLinearFunction::add(const Eigen::MatrixBase<Derived> & A, VariablePtr x)
{
  if(!x->space().isEuclidean() && x->isBasePrimitive())
    throw std::runtime_error("We allow linear function only on Euclidean variables.");
  if(A.rows() != size())
    throw std::runtime_error("Matrix A doesn't have coherent row size.");
  if(A.cols() != x->size())
    throw std::runtime_error("Matrix A doesn't have its column size coherent with its corresponding variable.");

  if(variables().contains(*x))
  {
    auto J = jacobian_.at(x.get(), tvm::utils::internal::with_sub{});
    J += A;                                                           // this erase the properties of J;
    J.properties({tvm::internal::MatrixProperties::Constness(true)}); // We restore only this one
  }
  else
  {
    using Shape = tvm::internal::MatrixProperties::Shape;
    addVariable(x, true);
    jacobian_.at(x.get()) = A;
    Shape shape = Shape::GENERAL;
    // TODO: generalize test to matrices with fixed size, to triangular matrices
    // and to richer diagonal case
    if constexpr(std::is_same_v<Derived, std::remove_const_t<tvm::utils::internal::IdentityType>>)
    {
      shape = Shape::IDENTITY;
    }
    if constexpr(std::is_same_v<Derived, std::remove_const_t<tvm::utils::internal::MinusIdentityType>>)
    {
      shape = Shape::MINUS_IDENTITY;
    }
    if constexpr(std::is_same_v<Derived, std::remove_const_t<tvm::utils::internal::MultIdentityType>>)
    {
      shape = Shape::MULTIPLE_OF_IDENTITY;
    }
    if constexpr(std::is_same_v<Derived, std::remove_const_t<decltype(Eigen::VectorXd(1).asDiagonal())>>)
    {
      shape = Shape::DIAGONAL;
    }
    jacobian_.at(x.get()).properties({tvm::internal::MatrixProperties::Constness(true), shape});
  }
}

template<typename Tuple, size_t... Indices>
inline void BasicLinearFunction::add(Tuple && tuple, std::index_sequence<Indices...>)
{
  (add(std::get<Indices>(std::forward<Tuple>(tuple))), ...);
}

template<typename Derived>
inline void BasicLinearFunction::add(const utils::LinearExpr<Derived> & lin)
{
  add(lin.matrix(), lin.variable());
}

} // namespace function

} // namespace tvm
