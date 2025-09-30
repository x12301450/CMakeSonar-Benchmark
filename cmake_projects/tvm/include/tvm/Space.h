/** Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#pragma once

#include <tvm/api.h>
#include <tvm/defs.h>

#include <Eigen/Core>

#include <memory>
#include <string>
#include <vector>

namespace tvm
{
/** Description of a variable space, and factory for Variable.
 *
 * The space can have up to 3 different sizes, although Euclidean spaces will
 * have only one :
 * - the size of the space as a manifold,
 * - the size of the vector needed to represent one variable (point) in this
 *   space (representation space, rsize),
 * - the size of the vector needed to represent a derivative (velocity,
 *   acceleration, ...) of this variable (tangent space, tsize).
 *
 * Here are a few examples:
 * - R^n has a size, rsize and tsize of n,
 * - SO(3), the 3d rotation space, when represented by quaternions, has a
 *   size of 3, a rsize of 4 and a tsize of 3,
 * - S(2), the sphere in dimension 3 has a size of 2, and rsize and tsize of 3.
 */
class TVM_DLLAPI Space
{
public:
  /** Predefined space types.*/
  enum class Type
  {
    Euclidean,  /** Euclidean space \f$ \mathbb{R}^n \f$.*/
    SO3,        /** Space of 3d rotations, represented by unit quaternions.*/
    SE3,        /** Space of 3d transformation, represented by a quaternion and a 3d-vector.*/
    Unspecified /** Non-euclidean space of unknown type.*/
  };

  /** Constructor for an Euclidean space
   *
   * \param size size of the space
   */
  Space(int size);
  /** Constructor for a manifold with tsize = size
   *
   * \param size size of the space
   * \param representationSize size of the vector needed to represent a variable
   */
  Space(int size, int representationSize);
  /** Constructor for a manifold where tsize != size
   *
   * \param size size of the space
   * \param representationSize size of the vector needed to represent a variable
   * \param tangentRepresentationSize size of the vector needed to represent a derivative
   */
  Space(int size, int representationSize, int tangentRepresentationSize);

  /** Constructor for a given space type
   *
   * \param type type of space
   * \param size size of the space. Only for space types whose size is not fixed.
   */
  Space(Type type, int size = -1);

  /** Factory function to create a variable.*/
  std::unique_ptr<Variable> createVariable(std::string_view name) const;

  /** Size of the space (as a manifold) */
  int size() const;
  /** Size of the vector needed to represent a variable in this space.*/
  int rSize() const;
  /** Size of the vector needed to represent a derivative in this space.*/
  int tSize() const;
  /** Type of the space. */
  Type type() const;
  /** Check if this is an Euclidean space.*/
  bool isEuclidean() const;
  /** Return size triplet as string*/
  std::string sizeAsString() const;

  bool operator==(const Space & other) const
  {
    return this->mSize_ == other.mSize_ && this->rSize_ == other.rSize_ && this->tSize_ == other.tSize_
           && this->type_ == other.type_;
  }

  bool operator!=(const Space & other) const { return !operator==(other); }

  bool operator<=(const Space & other) const
  {
    return this->mSize_ <= other.mSize_ && this->rSize_ <= other.rSize_ && this->tSize_ <= other.tSize_;
  }

  friend Space operator*(const Space & s1, const Space & s2)
  {
    Space prod{s1.mSize_ + s2.mSize_, s1.rSize_ + s2.rSize_, s1.tSize_ + s2.tSize_};
    if(s1.type_ == Space::Type::Euclidean && s2.type_ == Space::Type::Euclidean)
      prod.type_ = Space::Type::Euclidean;
    else
      prod.type_ = Space::Type::Unspecified;
    return prod;
  }

private:
  int mSize_; // size of this space (as a manifold)
  int rSize_; // size of a vector representing a point in this space
  int tSize_; // size of a vector representing a velocity in this space
  Type type_; // the space type
};

} // namespace tvm
