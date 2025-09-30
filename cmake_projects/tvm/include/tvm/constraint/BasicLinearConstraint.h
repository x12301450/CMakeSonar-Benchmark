/** Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#pragma once

#include <tvm/constraint/abstract/LinearConstraint.h>
#include <tvm/internal/MatrixProperties.h>

namespace tvm
{

namespace constraint
{

/** The most basic linear constraint where the matrix and the vector(s)
 * are constant.
 */
class TVM_DLLAPI BasicLinearConstraint : public abstract::LinearConstraint
{
public:
  /** Ax = 0, Ax <= 0 or Ax >= 0. */
  BasicLinearConstraint(const MatrixConstRef & A, VariablePtr x, Type ct);

  /** A_{i}x_{i} = 0, A_{i}x_{i} <= 0 or A_{i}x_{i} >= 0 */
  BasicLinearConstraint(const std::vector<MatrixConstRef> & A, const std::vector<VariablePtr> & x, Type ct);

  /** Ax = +/-b, Ax <= +/-b or Ax >= +/-b */
  BasicLinearConstraint(const MatrixConstRef & A,
                        VariablePtr x,
                        const VectorConstRef & b,
                        Type ct,
                        RHS cr = RHS::AS_GIVEN);

  /** A_{i}x_{i} = +/-b, A_{i}x_{i} <= +/-b or A_{i}x_{i} >= +/-b */
  BasicLinearConstraint(const std::vector<MatrixConstRef> & A,
                        const std::vector<VariablePtr> & x,
                        const VectorConstRef & b,
                        Type ct,
                        RHS cr = RHS::AS_GIVEN);

  /** l <= Ax <= u */
  BasicLinearConstraint(const MatrixConstRef & A,
                        VariablePtr x,
                        const VectorConstRef & l,
                        const VectorConstRef & u,
                        RHS cr = RHS::AS_GIVEN);

  /** l <= A_{i}x_{i} <= u */
  BasicLinearConstraint(const std::vector<MatrixConstRef> & A,
                        const std::vector<VariablePtr> & x,
                        const VectorConstRef & l,
                        const VectorConstRef & u,
                        RHS cr = RHS::AS_GIVEN);

  /** Uninitialized data. Allocate memory for a constraint with \p m rows. */
  BasicLinearConstraint(int m, VariablePtr x, Type ct, RHS cr = RHS::AS_GIVEN);
  /** Uninitialized data. Allocate memory for a constraint with \p m rows. */
  BasicLinearConstraint(int m, std::vector<VariablePtr> & x, Type ct, RHS cr = RHS::AS_GIVEN);

  /** Set the matrix \p A corresponding to variable \p x.
   * Optionally set the properties of \p A with \p p
   */
  void A(const MatrixConstRef & A,
         const Variable & x,
         const tvm::internal::MatrixProperties & p = tvm::internal::MatrixProperties());
  /** Shortcut for
   *void A(const MatrixConstRef&, const Variable&, const tvm::internal::MatrixProperties&)
   * when there is a single variable.
   */
  void A(const MatrixConstRef & A, const tvm::internal::MatrixProperties & p = tvm::internal::MatrixProperties());
  /** Set \p b */
  void b(const VectorConstRef & b);
  using abstract::LinearConstraint::l;
  /** Set \p l */
  void l(const VectorConstRef & l);
  using abstract::LinearConstraint::u;
  /** Set \p u */
  void u(const VectorConstRef & u);

private:
  void add(const Eigen::MatrixXd & A, VariablePtr x);
};

} // namespace constraint

} // namespace tvm
