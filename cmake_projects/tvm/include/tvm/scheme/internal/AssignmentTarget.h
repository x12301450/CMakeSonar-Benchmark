/** Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#pragma once

#include <tvm/api.h>
#include <tvm/defs.h>

#include <tvm/constraint/enums.h>

#include <Eigen/Core>

#include <memory>

namespace tvm
{

namespace scheme
{

namespace internal
{

enum class TargetType
{
  Linear,
  Quadratic
};

/** This class describes the matrix and vector(s) rows in which a given
 * constraint needs to be copied, and the convention to be used for those
 * matrix and vectors.
 *
 * If the target is quadratic form, the whole matrix and vector are
 * returned.
 */
class TVM_DLLAPI AssignmentTarget
{
public:
  /** Ax = 0, Ax <= 0 or Ax >= 0. */
  AssignmentTarget(RangePtr range, MatrixRef A, constraint::Type ct);
  /** Ax = +/-b, Ax <= +/-b or Ax >= +/-b */
  AssignmentTarget(RangePtr range, MatrixRef A, VectorRef b, constraint::Type ct, constraint::RHS cr);

  /** l <= Ax <= u */
  AssignmentTarget(RangePtr range, MatrixRef A, VectorRef l, VectorRef u, constraint::RHS cr);

  /** l <= x <= u */
  AssignmentTarget(RangePtr range, VectorRef l, VectorRef u);

  /** x >= b or x <= b*/
  AssignmentTarget(RangePtr range, VectorRef lu, constraint::Type ct);

  /** Quadratic function 1/2 x^T Q x +\epsilon q, where \epsilon = 0, 1 or -1 depending on cr.*/
  AssignmentTarget(MatrixRef Q, VectorRef q, constraint::RHS cr);

  TargetType targetType() const;
  constraint::Type constraintType() const;
  constraint::RHS constraintRhs() const;
  /** Row size of the target.*/
  int size() const;
  Range & range();
  const Range & range() const;

  /** Return the (range.dim x colDim) block of A starting at
   *(range.start,colStart) */
  MatrixRef A(int colStart, int colDim) const;
  /** Return the whole quadratic matrix*/
  MatrixRef Q() const;
  /** Return the segment of l defined by range. */
  VectorRef l() const;
  /** Return the segment of u defined by range. */
  VectorRef u() const;
  /** Return the segment of b defined by range. */
  VectorRef b() const;
  /** Return the whole vector q.*/
  VectorRef q() const;

  /** Same as A(...), and b(), but return only the first or second half of
   * the row range. This is necessary when double-sided constraints are
   * assigned to matrix/vector with single-sided convention
   */
  /**@{*/
  MatrixRef AFirstHalf(int colStart, int colDim) const;
  MatrixRef ASecondHalf(int colStart, int colDim) const;
  VectorRef bFirstHalf() const;
  VectorRef bSecondHalf() const;
  /**@}*/

  AssignmentTarget & setA(MatrixRef A);
  AssignmentTarget & setQ(MatrixRef Q);
  AssignmentTarget & setl(VectorRef l);
  AssignmentTarget & setu(VectorRef u);
  AssignmentTarget & setb(VectorRef b);
  AssignmentTarget & setq(VectorRef q);

  /** Ax = +/-b, Ax <= +/-b, Ax >= +/-b or 1/2 x^T Q x +\epsilon q*/
  void changeData(MatrixRef AQ, VectorRef bq);

  /** l <= Ax <= u */
  void changeData(MatrixRef A, VectorRef l, VectorRef u);

  /** l <= x <= u */
  void changeData(VectorRef l, VectorRef u);

private:
  /** Type of target*/
  TargetType targetType_;
  /** Constraint type convention*/
  constraint::Type cstrType_;
  /** RHS type convention*/
  constraint::RHS constraintRhs_;
  /** Pointer to the row range*/
  RangePtr range_;
  /** Pointers to the target matrix and vectors (when applicable) */
  MatrixRef A_ = Eigen::Map<Eigen::MatrixXd>(nullptr, 0, 0);
  MatrixRef Q_ = Eigen::Map<Eigen::MatrixXd>(nullptr, 0, 0);
  VectorRef l_ = Eigen::Map<Eigen::VectorXd>(nullptr, 0);
  VectorRef u_ = Eigen::Map<Eigen::VectorXd>(nullptr, 0);
  VectorRef b_ = Eigen::Map<Eigen::VectorXd>(nullptr, 0);
  VectorRef q_ = Eigen::Map<Eigen::VectorXd>(nullptr, 0);
};

} // namespace internal

} // namespace scheme

} // namespace tvm
