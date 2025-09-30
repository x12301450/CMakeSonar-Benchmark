/** Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#pragma once

#include <tvm/Range.h>
#include <tvm/Space.h>

#include <tvm/internal/ObjWithId.h>

#include <tvm/deprecated.hh>

#include <Eigen/Core>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tvm
{
class Variable;
class VariableVector;

/** Get the ndiff-th time derivative of a variable
 *
 * \param var the variable to be derived
 * \param ndiff the order of the derivation
 * \param autoName base the name on that of the supervariable of the primitive
 * (if the variable was not already created with another name before).
 */
VariablePtr TVM_DLLAPI dot(VariablePtr var, int ndiff = 1, bool autoName = false);

/** Representation of a Variable, i.e. a point in a space.
 *
 * A variable can only be constructed from a Space, in which case we say it
 * is a base primitive, or from an other variable, by derivation with the
 * dot() operator.
 *
 * A variables can be a subvariable, i.e. a contiguous subpart of an other variable.
 *
 * \internal A note on memory management for variables: Variable instances can
 * only be created through factory function returning shared_ptr (unique_ptr for
 * initial variables created from Space). Derived variables and subvariables
 * share the same reference counter, through the aliasing mechanism of shared_ptr.
 * This ensure that the lifetime of a variable, and in particular the original
 * variable always exceed that of a variable constructed from it.
 */
class TVM_DLLAPI Variable : public tvm::internal::ObjWithId, public std::enable_shared_from_this<Variable>
{
private:
  struct make_shared_token
  {};

public:
  /** Copying variables is illicit. To get a different variable with the same
   * characteristics, see \ref duplicate.
   */
  Variable(const Variable &) = delete;
  /** Copying variables is illicit. To get a different variable with the same
   * characteristics, see \ref duplicate.
   */
  Variable & operator=(const Variable &) = delete;
  /** Destructor.*/
  ~Variable();
  /** Create a variable based on the same space, with the same derivative
   * number and value.
   * \param name the name of the new variable. If the default value is kept,
   * a ' will be appended to the current name. If the variable being
   * duplicated is not a base variable, the name is given to the base
   * variable.
   */
  VariablePtr duplicate(std::string_view name = "") const;
  /** Return the name of the variable*/
  const std::string & name() const;
  /** Return the size of the variable, i.e. the size of the vector needed to
   * represent it.
   */
  int size() const;
  /** Return the space to which this variable is linked. It is either the
   * space the variable was obtained from (for a variable that is a
   * primitive), or the space of its primitive (for a variable which is
   * derived from an other).
   */
  const Space & space() const;
  /** Return the space of the subvariable preceding this variable w.r.t its
   * supervariable. E.g if this variable is x, and is a subvariable of X such
   * that X = (u, x, y), x->spaceShift() corresponds to u->space().
   */
  const Space & spaceShift() const;
  /** Check if the variable lives in a Euclidean space.
   * Pay attention that for a variable v, v.isEuclidean() is not necessarily
   * equal to v.space().isEuclidean(): if v is a derivative of a variable linked
   * to a non-Euclidean space, it lives in the tangent space which is Euclidean.
   */
  bool isEuclidean() const;
  /** Return the current value of the variable.*/
  VectorConstRef value() const;
  /** (DEPRECATED) Set the value of the variable.*/
  TVM_DEPRECATED inline void value(const VectorConstRef & x) { set(x); }
  /** Set the value of the variable.*/
  void set(const VectorConstRef & x);
  /** Set the value at \p idx to \p value */
  void set(Eigen::DenseIndex idx, double value);
  /** Set the value of a block of length \p length starting at \p idx to the value in \p value */
  void set(Eigen::DenseIndex idx, Eigen::DenseIndex length, const VectorConstRef & value);
  /** Set the value of the variable to 0*/
  void setZero();
  /** If this variable is a base primitive (i.e. built from a space), return 0.
   * Otherwise, return the number of time a base primitive variable had to be
   * derived to get to this variable.
   */
  int derivativeNumber() const;
  /** Check if this variable is a derivative of \p v
   * This is in a strict sense: v.isDerivativeOf(v) si false.
   */
  bool isDerivativeOf(const Variable & v) const;
  /** Check if this variable is a primitive of \p v
   * This is in a strict sense: v.isPrimitiveOf(v) si false.
   */
  bool isPrimitiveOf(const Variable & v) const;
  /** Return true if this variable is a base primitive (derivativeNumber() == 0),
   * false otherwise.
   */
  bool isBasePrimitive() const;
  /** Get the n-th primitive of the variable*/
  template<int n = 1>
  VariablePtr primitive() const;
  /** Get the base primitive of this variable. Equivalent to primitive<d>()
   * where d = derivativeNumber().
   */
  VariablePtr basePrimitive() const;

  /** Return whether or not this variable is the subvariable of another variable.*/
  bool isSubvariable() const;
  /** If this variable is a subvariable, return the variable it is a subvariable
   * of. Otherwise, return this variable.*/
  VariablePtr superVariable() const;
  /** Return true if this variable is not the subvariable of any other variable.*/
  bool isSuperVariable() const;
  /** Return true if \p v is a subvariable of this variable and this variable is
   * a supervariable.
   *
   * The requirement that this variable is a supervariable makes it different
   * from Variable::contains.
   */
  bool isSuperVariableOf(const Variable & v) const;
  /** If this variable is a subvariable, return its range within its supevariable.
   * If not, simply return [0, size()].
   */
  Range subvariableRange() const;
  /** Equivalent to dot(this)->subvariableRange (without having to create the
   * derivative).
   */
  Range tSubvariableRange() const;
  /** Return true if \p v is a sub-part of this variable.
   *
   * To the difference of Variable::isSuperVariableOf, it is sufficient that
   * both this variable and \p v have the same supervariable. This variable does
   * not need to be a supervariable itself.
   */
  bool contains(const Variable & v) const;
  /** Return this variable and v both contain the same subpart of a variable.*/
  bool intersects(const Variable & v) const;

  /** Create a subvariable
   *
   * \param space The space in which the variable (or its base primitive in case
   * of a derivative) is a point.
   * \param baseName The name of the variable or its base primitive (if
   * different).
   * \param shift The space of the subvariable before this subvariable w.r.t the
   * current variable.
   * For example \c x->subVariable(Space(3), "y", Space(2)) creates a
   * subvariable y of \c x starting after the two first element (x0, x1) and
   * containing the three elements (x2,x3,x4).
   * This shift is with respect to the space of the supervariable. This is
   * important if it is not Euclidean.
   */
  [[nodiscard]] VariablePtr subvariable(Space space, std::string_view baseName, Space shift = {0}) const;

  /** Create a subvariable
   *
   * \param space The space in which the variable (or its base primitive in case
   * of a derivative) is a point.
   * \param shift The space of the subvariable before this subvariable w.r.t the
   * current variable.
   * For example \c x->subVariable(Space(3), "y", Space(2)) creates a
   * subvariable y of \c x starting after the two first element (x0, x1) and
   * containing the three elements (x2,x3,x4).
   * This shift is with respect to the space of the supervariable. This is
   * important if it is not Euclidean.
   *
   * Name is automatically generated as x[start:end] (or dk x/ dtk [start:end])
   * for a k-th derivative.
   */
  [[nodiscard]] VariablePtr subvariable(Space space, Space shift = {0}) const;

  /** Get the mapping of this variable, within a vector of variables: if all
   * variables values are stacked in one vector v, the returned Range
   * specifies the segment of v corresponding to this variable.
   */
  Range getMappingIn(const VariableVector & variables) const;

  /** Overload of enable_shared_from_this::shared_from_this, for technical purpose.
   *
   * \internal This is necessary because all derivative and subvariables are
   * sharing the same ref counter as the primitive supervariable, while the
   * default shared_from_this use the variable own (and unused) ref counter.
   */
  [[nodiscard]] VariablePtr shared_from_this();

  /** Helper to initialize a variable like an Eigen vector.*/
  Eigen::CommaInitializer<VectorRef> operator<<(double d);

  /** Helper to initialize a variable like an Eigen vector.*/
  template<typename Derived>
  Eigen::CommaInitializer<VectorRef> operator<<(const Eigen::DenseBase<Derived> & other);

  friend bool operator==(const Variable & u, const Variable & v);
  friend bool operator!=(const Variable & u, const Variable & v);

  /** Constructor for a subvariable of var */
  Variable(make_shared_token, VariablePtr var, const Space & space, std::string_view name, const Space & shift);

private:
  struct MappingHelper
  {
    int start;
    int stamp;
  };

  /** Constructor for a new variable */
  Variable(const Space & s, std::string_view name);

  /** Constructor for the derivative of var
   * \param autoName base the name on that of the supervariable of the primitive
   * (if the variable was not already created with another name before).
   */
  Variable(Variable * var, bool autoName);

  /** Subfunction for the two public overloads. The goal of these overload is to
   * because autoName = true makes the method effectively ignore baseName.
   */
  [[nodiscard]] VariablePtr subvariable(Space space, std::string_view baseName, Space shift, bool autoName) const;

  /** Same as primitive<n> but without checking if n is valid.*/
  template<int n>
  VariablePtr primitiveNoCheck() const;

  /** Name */
  std::string name_;

  /** Data of the space from which the variable is a point. */
  Space space_;

  /** For a variable x that is a subvariable of X = (x-, x, x+), specify the
   * space of x-.
   */
  Space shift_;

  /** Buffer for the variable value. */
  double * memory_;

  /** Value of the variable. */
  VectorRef value_;

  /** Number of derivation since the base primitive. 0 a variable which is
   * not a derivative.
   */
  int derivativeNumber_;

  /** If the variable is the time derivative of another one, primitive_ is a
   * reference to the latter, otherwise it is a nullptr.
   */
  Variable * primitive_;

  /** If the variable is the subvariable of another one, superVariable_ is a
   * reference to the latter, otherwise it is a nullptr.
   */
  VariablePtr superVariable_;

  /** If the variable has a time derivative, keep a pointer on it */
  std::unique_ptr<Variable> derivative_;

  /** The starting position of this variable w.r.t. a VariableVector identified
   * by its id
   */
  mutable std::unordered_map<int, MappingHelper> startIn_;

  /** friendship declaration */
  friend class Space;
  friend VariablePtr TVM_DLLAPI dot(VariablePtr, int, bool);
  friend class VariableVector;
};

template<int n>
inline VariablePtr Variable::primitive() const
{
  if(n <= derivativeNumber_)
    return primitiveNoCheck<n>();
  else
    throw std::runtime_error("This variable is not the n-th derivative of an other variable.");
}

template<int n>
inline VariablePtr Variable::primitiveNoCheck() const
{
  static_assert(n > 0, "Works only for non-negative numbers.");
  return primitive_->primitive<n - 1>();
}

template<>
inline VariablePtr Variable::primitiveNoCheck<1>() const
{
  if(derivativeNumber_ > 1)
    return {basePrimitive(), primitive_};
  else
    return primitive_->shared_from_this();
}

inline Eigen::CommaInitializer<VectorRef> Variable::operator<<(double d) { return {value_, d}; }

template<typename Derived>
inline Eigen::CommaInitializer<VectorRef> Variable::operator<<(const Eigen::DenseBase<Derived> & other)
{
  return {value_, other};
}

inline bool operator==(const Variable & u, const Variable & v)
{
  return u.value_.data() == v.value_.data() && u.size() == v.size();
}

inline bool operator!=(const Variable & u, const Variable & v) { return !(u == v); }

} // namespace tvm

/** Helper to initialize a variable like an Eigen vector, with a coma separated list.*/
inline Eigen::CommaInitializer<tvm::VectorRef> operator<<(tvm::VariablePtr & v, double d) { return *v.get() << d; }

/** Helper to initialize a variable like an Eigen vector, with a coma separated list.*/
inline Eigen::CommaInitializer<tvm::VectorRef> operator<<(tvm::VariablePtr && v, double d) { return *v.get() << d; }

/** Helper to initialize a variable like an Eigen vector, with a coma separated list.*/
template<typename Derived>
inline Eigen::CommaInitializer<tvm::VectorRef> operator<<(tvm::VariablePtr & v, const Eigen::DenseBase<Derived> & other)
{
  return *v.get() << other;
}

/** Helper to initialize a variable like an Eigen vector, with a coma separated list.*/
template<typename Derived>
inline Eigen::CommaInitializer<Eigen::VectorXd> operator<<(tvm::VariablePtr && v,
                                                           const Eigen::DenseBase<Derived> & other)
{
  return *v.get() << other;
}
