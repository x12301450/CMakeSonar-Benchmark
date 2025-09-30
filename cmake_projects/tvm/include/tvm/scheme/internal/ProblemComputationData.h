/* Copyright 2017-2020 CNRS-AIST JRL and CNRS-UM LIRMM */

#pragma once

#include <tvm/api.h>
#include <tvm/defs.h>

#include <tvm/VariableVector.h>
#include <tvm/internal/VariableCountingVector.h>
#include <tvm/scheme/internal/ProblemDefinitionEvent.h>

#include <queue>
#include <vector>

namespace tvm::scheme::internal
{

/** Base class to be derived by each resolution scheme to hold all of the
 * temporary memories and states related to the resolution of a problem.
 * The rationale is that a resolution scheme is stateless, and when given
 * for the first time a problem to solve, creates a ProblemComputationData
 * that it gives to the problem and retrieves at each further resolution.
 * A problem computation data further act as a queue of events changing the
 * problem definition.
 */
class TVM_DLLAPI ProblemComputationData
{
public:
  class EventQueue : public std::queue<ProblemDefinitionEvent>
  {
  public:
    using std::queue<ProblemDefinitionEvent>::queue;

    using std::queue<ProblemDefinitionEvent>::c;
  };

  virtual ~ProblemComputationData() = default;

  int solverId() const;

  /** Add a variable if not already present. Return true if the variable was
   * effectively added. The class counts how many time a variable was added.
   */
  bool addVariable(VariablePtr var);
  /** Add a set of variables, calling addVariable(VariablePtr var) on each
   * variable.
   */
  void addVariable(const VariableVector & vars);
  /** Effectively remove a variable if as many calls were made to this method
   * as to addVariable, for the given variable. Return true if the variable
   * was really removed.
   */
  bool removeVariable(Variable * v);
  /** Remove a set of variables, calling removeVariable(Variable* v) on each
   * variable.
   */
  void removeVariable(const VariableVector & vars);
  /** Get the variables of the problem.*/
  const VariableVector & variables() const;

  /** Set the value of the variables to that of the solution. */
  void setVariablesToSolution();

  /** Adding an event changing the problem definition, that needs to be handled
   * by the resolution scheme. Events are stored in a queue (FIFO).
   */
  void addEvent(const ProblemDefinitionEvent & e);
  /** Read and remove the first event in the queue. Need the queue to be non-empty.*/
  ProblemDefinitionEvent popEvent();
  /** Is the queue not empty?*/
  bool hasEvents() const;
  /** List of events.*/
  const EventQueue::container_type & events() const;
  /** Clear the list of events.*/
  void clearEvents();
  /** Clear the variable vector.*/
  void clearVariables();

protected:
  ProblemComputationData(int solverId);
  ProblemComputationData() = delete;

  /** Need to put in x the solution of the computation. */
  virtual void setVariablesToSolution_(tvm::internal::VariableCountingVector & x) = 0;

  /** The problem variable*/
  tvm::internal::VariableCountingVector variables_;

  /** Problem definition events*/
  EventQueue events_;

private:
  int solverId_;
};

inline int ProblemComputationData::solverId() const { return solverId_; }

inline bool ProblemComputationData::addVariable(VariablePtr var) { return variables_.add(var); }

inline void ProblemComputationData::addVariable(const VariableVector & vars)
{
  for(const auto & v : vars.variables())
    addVariable(v);
}

inline bool ProblemComputationData::removeVariable(Variable * v) { return variables_.remove(*v); }

inline void ProblemComputationData::removeVariable(const VariableVector & vars)
{
  for(const auto & v : vars.variables())
    removeVariable(v.get());
}

inline const VariableVector & ProblemComputationData::variables() const { return variables_.variables(); }

inline void ProblemComputationData::setVariablesToSolution() { setVariablesToSolution_(variables_); }

inline void ProblemComputationData::addEvent(const ProblemDefinitionEvent & e)
{
  // By default, the event will be effectively added.
  bool add = true;

  // If this event is a task removal, it is useless to process other events that
  // would be in the event queue for the same task. We remove those events from
  // the queue.
  if(e.type() == ProblemDefinitionEvent::Type::TaskRemoval)
  {
    for(auto it = events_.c.begin(); it != events_.c.end();)
    {
      if(it->emitter() == e.emitter() && it->type() != ProblemDefinitionEvent::Type::TaskRemoval)
      {
        // In case the addition of the same task is in the event queue: since
        // we are removing it, it is not necessary to add the removal event.
        if(it->type() == ProblemDefinitionEvent::Type::TaskAddition)
        {
          assert(add && "The task was added twice");
          add = false;
        }
        it = events_.c.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
  if(add)
  {
    events_.push(e);
  }
}

[[nodiscard]] inline ProblemDefinitionEvent ProblemComputationData::popEvent()
{
  assert(hasEvents());
  auto e = events_.front();
  events_.pop();
  return e;
}

inline bool ProblemComputationData::hasEvents() const { return !events_.empty(); }

inline const ProblemComputationData::EventQueue::container_type & ProblemComputationData::events() const
{
  return events_.c;
}

inline void ProblemComputationData::clearEvents() { events_ = {}; }

inline void ProblemComputationData::clearVariables() { variables_.clear(); }

inline ProblemComputationData::ProblemComputationData(int solverId) : solverId_(solverId) {}
} // namespace tvm::scheme::internal
