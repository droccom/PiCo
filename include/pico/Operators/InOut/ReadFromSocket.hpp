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
/*
 * ReadFromSocket.hpp
 *
 *  Created on: Dec 13, 2016
 *      Author: misale
 */

#ifndef OPERATORS_INOUT_READFROMSOCKET_HPP_
#define OPERATORS_INOUT_READFROMSOCKET_HPP_

#include <fstream>
#include <iostream>

#include "pico/ff_implementation/OperatorsFFNodes/InOut/ReadFromSocketFFNode.hpp"

#include "InputOperator.hpp"

namespace pico {

/**
 * Defines an operator that reads a data stream from a socket,
 * yielding an ordered unbounded collection.
 *
 * The user specifies a delimiter to identify stream items.
 */

class ReadFromSocket : public InputOperator<std::string> {
 public:
  /**
   * \ingroup op-api
   * ReadFromSocket Constructor
   *
   * Creates a new ReadFromSocket operator by defining its kernel function,
   * operating on each token of the stream, delimited by the delimiter value.
   */
  ReadFromSocket(const std::string& server_, int port_, char delimiter_)
      : InputOperator<std::string>(StructureType::STREAM), server_name(server_) {
    port = port_;
    delimiter = delimiter_;
  }

  /**
   * Copy constructor.
   */
  ReadFromSocket(const ReadFromSocket &copy)
      : InputOperator<std::string>(copy), server_name(copy.server_name) {
    port = copy.port;
    delimiter = copy.delimiter;
  }

  /**
   * Returns a unique name for the operator.
   */
  std::string name() {
    std::string name("ReadFromSocket");
    std::ostringstream address;
    address << (void const *)this;
    return name + address.str().erase(0, 2);
  }

  /**
   * Returns the name of the operator, consisting in the name of the class.
   */
  std::string name_short() { return "ReadFromSocket\n[" + server_name + "]"; }

 protected:
  ReadFromSocket *clone() { return new ReadFromSocket(*this); }

  const OpClass operator_class() { return OpClass::INPUT; }

  ff::ff_node *node_operator(int parallelism, StructureType st) {
    assert(st == StructureType::STREAM);
    return new ReadFromSocketFFNode(server_name, port, delimiter);
  }

 private:
  std::string server_name;
  int port;
  char delimiter;
};

} /* namespace pico */

#endif /* OPERATORS_INOUT_READFROMSOCKET_HPP_ */
