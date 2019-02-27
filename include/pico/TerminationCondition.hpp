/*
 * Copyright (c) 2019 alpha group, CS department, University of Torino.
 *
 * This file is part of pico
 * (see https://github.com/alpha-unito/pico).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PICO_TERMINATIONCONDITION_HPP_
#define PICO_TERMINATIONCONDITION_HPP_

#include "pico/ff_implementation/iteration/fixed_length.hpp"

namespace pico {

class TerminationCondition {
 public:
  virtual ~TerminationCondition() {}

  virtual base_switch *iteration_switch() = 0;
  virtual TerminationCondition *clone() = 0;
};

class FixedIterations : public TerminationCondition {
 public:
  explicit FixedIterations(unsigned iters_) : iters(iters_) {}

  FixedIterations(const FixedIterations &copy) : iters(copy.iters) {}

  base_switch *iteration_switch() {
    return new fixed_length_iteration_dispatcher(iters);
  }

  FixedIterations *clone() { return new FixedIterations(*this); }

 private:
  unsigned iters;
};

} /* namespace pico */

#endif /* PICO_TERMINATIONCONDITION_HPP_ */
