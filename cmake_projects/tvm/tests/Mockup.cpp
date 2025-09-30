/** Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#include "Mockup.h"
#include <string>

RobotMockup::RobotMockup() : k(0), v(0), d(0), a(0)
{
  // clang-format off
  registerUpdates(Update::Kinematics, &RobotMockup::updateK,
                  Update::Velocity, &RobotMockup::updateV,
                  Update::Dynamics, &RobotMockup::updateD,
                  Update::Acceleration, &RobotMockup::updateA);
  // clang-format on

  addOutputDependency({Output::K1, Output::K2, Output::K3}, Update::Kinematics);
  addOutputDependency({Output::V1, Output::V2}, Update::Velocity);
  addOutputDependency({Output::D1, Output::D2}, Update::Dynamics);
  addOutputDependency(Output::A1, Update::Acceleration);

  addInternalDependency(Update::Velocity, Update::Kinematics);
  addInternalDependency(Update::Dynamics, Update::Velocity);
  addInternalDependency(Update::Acceleration, Update::Velocity);
}

FunctionMockup::FunctionMockup()
{
  // clang-format off
  registerUpdates(Update::Value, &FunctionMockup::updateValue,
                  Update::Velocity, &FunctionMockup::updateVelocity,
                  Update::JDot, &FunctionMockup::updateJDot);
  // clang-format on
}

RobotFunction::RobotFunction(std::shared_ptr<RobotMockup> robot) : robot_(robot)
{
  addInput(robot, RobotMockup::Output::K1, RobotMockup::Output::K2, RobotMockup::Output::K3, RobotMockup::Output::V1,
           RobotMockup::Output::V2, RobotMockup::Output::D1, RobotMockup::Output::D2);

  addInternalDependency(Update::Velocity, Update::Value);
  addInternalDependency(Update::JDot, Update::Velocity);

  addInputDependency(Update::Value, robot_, RobotMockup::Output::K1, RobotMockup::Output::K2, RobotMockup::Output::K3);
  addInputDependency(Update::Velocity, robot_, RobotMockup::Output::V1, RobotMockup::Output::V2);
  addInputDependency(Update::JDot, robot_, RobotMockup::Output::V1, RobotMockup::Output::V2);
}

SomeRobotFunction1::SomeRobotFunction1(std::shared_ptr<RobotMockup> robot) : RobotFunction(robot)
{
  addOutputDependency(Output::Value, Update::Value);
  addOutputDependency(Output::Jacobian, Update::Velocity);
  addOutputDependency(Output::Velocity, Update::Velocity);
  addOutputDependency(Output::NormalAcceleration, Update::Velocity);
  addOutputDependency(Output::JDot, Update::JDot);
}

void SomeRobotFunction1::updateValue()
{
  std::cout << "update SomeRobotFunction1::Value" << std::endl;
  val = (int)robot_->getK1() + (int)robot_->getK2();
}
void SomeRobotFunction1::updateVelocity()
{
  std::cout << "update SomeRobotFunction1::Velocity" << std::endl;
  j = (int)robot_->getV1();
  vel = (int)robot_->getV2();
  na = (int)robot_->getK3() * (int)robot_->getV1();
}
void SomeRobotFunction1::updateJDot()
{
  std::cout << "update SomeRobotFunction1::JDot" << std::endl;
  jdot = (int)robot_->getK3() * (int)robot_->getV1() + (int)robot_->getV2();
}

SomeRobotFunction2::SomeRobotFunction2(std::shared_ptr<RobotMockup> robot) : RobotFunction(robot)
{
  addOutputDependency(Output::Value, Update::Value);
  addOutputDependency(Output::Jacobian, Update::Velocity);
}

void SomeRobotFunction2::updateValue()
{
  val = robot_->getK1();
  std::cout << "update SomeRobotFunction2::Value" << std::endl;
}
void SomeRobotFunction2::updateVelocity()
{
  j = robot_->getV1();
  std::cout << "update SomeRobotFunction2::Velocity" << std::endl;
}

BadRobotFunction::BadRobotFunction(std::shared_ptr<RobotMockup> robot) : RobotFunction(robot)
{
  addOutputDependency(Output::Value, Update::Value);
  addOutputDependency(Output::Jacobian, Update::Velocity);
  addInternalDependency(Update::Velocity, Update::JDot); // BAAAAAD
}

LinearConstraint::LinearConstraint(const std::string & name) : name_(name)
{
  registerUpdates(Update::Matrices, &LinearConstraint::updateMatrices);

  addOutputDependency({Output::Value, Output::A, Output::b}, Update::Matrices);
}

Dummy<int(LinearConstraint::Output::Value)> LinearConstraint::value(int x) const
{
  // obviously, in real setting we would depend from a variable and take its value
  return A_ * x + b_;
}

Dummy<int(LinearConstraint::Output::A)> LinearConstraint::A() const { return A_; }

Dummy<int(LinearConstraint::Output::b)> LinearConstraint::b() const { return b_; }

KinematicLinearizedConstraint::KinematicLinearizedConstraint(const std::string & name,
                                                             std::shared_ptr<FunctionMockup> function)
: LinearConstraint(name), function_(function)
{
  addInput(function, FunctionMockup::Output::Value, FunctionMockup::Output::Jacobian);
  addInputDependency(Update::Matrices, function_, FunctionMockup::Output::Value, FunctionMockup::Output::Jacobian);
}

void KinematicLinearizedConstraint::updateMatrices()
{
  A_ = function_->jacobian();
  b_ = function_->value();
  std::cout << "update KinematicLinearizedConstraint::updateMatrices" << std::endl;
}

DynamicLinearizedConstraint::DynamicLinearizedConstraint(const std::string & name,
                                                         std::shared_ptr<FunctionMockup> function)
: LinearConstraint(name), function_(function)
{
  addInput(function, FunctionMockup::Output::Value, FunctionMockup::Output::Jacobian, FunctionMockup::Output::Velocity,
           FunctionMockup::Output::NormalAcceleration);

  addInputDependency(Update::Matrices, function_, FunctionMockup::Output::Value, FunctionMockup::Output::Jacobian,
                     FunctionMockup::Output::Velocity, FunctionMockup::Output::NormalAcceleration);
}

void DynamicLinearizedConstraint::updateMatrices()
{
  A_ = function_->jacobian();
  b_ = function_->value() + function_->velocity() + function_->normalAcc();
  std::cout << "update DynamicLinearizedConstraint::updateMatrices" << std::endl;
}

DynamicEquation::DynamicEquation(const std::string & name, std::shared_ptr<RobotMockup> robot)
: LinearConstraint(name), robot_(robot)
{
  addInput(robot, RobotMockup::Output::D1, RobotMockup::Output::D2);

  addInputDependency(Update::Matrices, robot_, RobotMockup::Output::D1, RobotMockup::Output::D2);
}

void DynamicEquation::updateMatrices()
{
  A_ = robot_->getD1();
  b_ = robot_->getD2();
  std::cout << "update DynamicEquation::updateMatrices" << std::endl;
}

BetterLinearConstraint::BetterLinearConstraint(const std::string & name) : name_(name) {}

Dummy<int(BetterLinearConstraint::Output::Value)> BetterLinearConstraint::value(int x) const
{
  // obviously, in real setting we would depend from a variable and take its value
  return A_ * x + b_;
}

Dummy<int(BetterLinearConstraint::Output::A)> BetterLinearConstraint::A() const { return A_; }

Dummy<int(BetterLinearConstraint::Output::b)> BetterLinearConstraint::b() const { return b_; }

BetterKinematicLinearizedConstraint::BetterKinematicLinearizedConstraint(const std::string & name,
                                                                         std::shared_ptr<FunctionMockup> function)
: BetterLinearConstraint(name), function_(function)
{
  addInput(function, FunctionMockup::Output::Value, FunctionMockup::Output::Jacobian);
  addDirectDependency(Output::A, function, FunctionMockup::Output::Jacobian);
  addDirectDependency(Output::b, function, FunctionMockup::Output::Value);
}

Dummy<int(BetterLinearConstraint::Output::b)> BetterKinematicLinearizedConstraint::b() const
{
  return static_cast<int>(function_->value());
}

Dummy<int(BetterLinearConstraint::Output::A)> BetterKinematicLinearizedConstraint::A() const
{
  return static_cast<int>(function_->jacobian());
}

BetterDynamicLinearizedConstraint::BetterDynamicLinearizedConstraint(const std::string & name,
                                                                     std::shared_ptr<FunctionMockup> function)
: BetterLinearConstraint(name), function_(function)
{
  registerUpdates(Update::Updateb, &BetterDynamicLinearizedConstraint::updateb);

  addInput(function, FunctionMockup::Output::Value, FunctionMockup::Output::Jacobian, FunctionMockup::Output::Velocity,
           FunctionMockup::Output::NormalAcceleration);

  addInputDependency<BetterDynamicLinearizedConstraint>(Update::Updateb, function_, FunctionMockup::Output::Value,
                                                        FunctionMockup::Output::Velocity,
                                                        FunctionMockup::Output::NormalAcceleration);

  addOutputDependency<BetterDynamicLinearizedConstraint>(Output::b, Update::Updateb);
  addDirectDependency(Output::A, function_, FunctionMockup::Output::Jacobian);
}

Dummy<int(BetterLinearConstraint::Output::A)> BetterDynamicLinearizedConstraint::A() const
{
  return static_cast<int>(function_->jacobian());
}

void BetterDynamicLinearizedConstraint::updateb()
{
  b_ = function_->value() + function_->velocity() + function_->normalAcc();
  std::cout << "update BetterDynamicLinearizedConstraint::updateb" << std::endl;
}
