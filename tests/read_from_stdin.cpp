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

#include <iostream>
#include <string>

#include <catch.hpp>

#include "pico/pico.hpp"

#include "common/io.hpp"

TEST_CASE("read from stdin and write", "read from stdin and write tag") {
  std::string input_file = "./testdata/lines.txt";
  std::string output_file = "output.txt";

  /* redirect input file to stdin */
  auto cinbuf = std::cin.rdbuf();  // save old buf
  std::ifstream in(input_file);
  std::cin.rdbuf(in.rdbuf());  // redirect

  /* redirect stdout to output file */
  auto coutbuf = std::cout.rdbuf();  // save old buf
  std::ofstream out(output_file);
  std::cout.rdbuf(out.rdbuf());  // redirect

  std::cin.tie(0);  // untie cin and cout

  /* define i/o operators from/to file */
  pico::ReadFromStdIn reader('\n');
  pico::WriteToStdOut<std::string> writer;

  /* compose the pipeline */
  auto io_file_pipe = pico::Pipe()  // the empty pipeline
                          .add(reader)
                          .add(writer);

  io_file_pipe.run();

  /* undo redirection */
  std::cout.rdbuf(coutbuf);
  std::cin.rdbuf(cinbuf);
  out.close();
  in.close();
  std::cin.tie(&std::cout);  // tie again cin and cout

  /* forget the order and compare */
  auto input_lines = read_lines(input_file);
  auto output_lines = read_lines(output_file);
  std::sort(input_lines.begin(), input_lines.end());
  std::sort(output_lines.begin(), output_lines.end());

  REQUIRE(input_lines == output_lines);
}
