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

#include <unordered_set>
#include <vector>

#include <catch2/catch.hpp>

#include "pico/pico.hpp"

#include "basic_pipes.hpp"
#include "common_functions.hpp"
#include "io.hpp"


typedef pico::KeyValue<char, int> KV;

typedef std::unordered_map<char, std::unordered_multiset<int>> KvMultiMap;

/* static flatmap kernel function
 *
 * if the pipe is not-empty, and if there are some KV with kv.Value() % 10 != 0,
 * the flatmap operator with this kernel function returns a not-empty Pipe.
 *
 */
static auto fltmp_kernel = [](KV& in, pico::FlatMapCollector<KV>& collector) {
  int val_mod = std::abs(in.Value()) % 10;
  if (val_mod % 10 != 0) {
    for (int i = 0; i <= val_mod; ++i) {
      collector.add(KV(in.Key(), in.Value() + 1));
      collector.add(KV(in.Key(), 0));
    }
  }  // else filters out
};

/*
 * sequential version of fltmp_kernel
 */
void fltmap_kernel_seq(std::vector<KV>& out, const std::vector<KV>& in) {
  out.clear();
  for (auto kv : in) {
    int val_mod = std::abs(kv.Value()) % 10;
    if (val_mod % 10 != 0) {
      for (int i = 0; i <= val_mod; ++i) {
        out.push_back(KV(kv.Key(), kv.Value() + 1));
        out.push_back(KV(kv.Key(), 0));
      }
    }  // else filters out
  }
}

/*
 * sequential version of the parallel iteration
 */

std::vector<std::string> seq_iter(std::vector<std::string> input_lines,
                                  int num_iter) {
  std::vector<std::string> res;
  std::vector<KV> collector, collector_;
  auto collector_ptr = &collector, collector_ptr_ = &collector_;
  for (auto line : input_lines) {
    collector.push_back(KV::from_string(line));
  }

  /* invalidate first swap */
  auto tmp = collector_ptr;
  collector_ptr = collector_ptr_;
  collector_ptr_ = tmp;

  for (int i = 0; i < num_iter; ++i) {
    tmp = collector_ptr;
    collector_ptr = collector_ptr_;
    collector_ptr_ = tmp;
    fltmap_kernel_seq(*collector_ptr_, *collector_ptr);
  }

  for (auto kv : *collector_ptr_) {
    res.push_back(kv.to_string());
  }

  return res;
}

TEST_CASE("iteration", "[iterationTag]") {
  std::string input_file = "./testdata/pairs.txt";
  std::string output_file = "outputIter.txt";

  pico::WriteToDisk<KV> writer(output_file,
                               [](KV in) { return in.to_string(); });

  const int num_iter = 3;
  pico::FixedIterations cond(num_iter);
  auto iter_pipe =
      pico::Pipe().add(pico::FlatMap<KV, KV>(fltmp_kernel)).iterate(cond);

  auto test_pipe = pipe_pairs_creator<KV>(input_file).to(iter_pipe).add(writer);

  test_pipe.run();

  auto observed = read_lines(output_file);
  std::sort(observed.begin(), observed.end());
  auto expected = seq_iter(read_lines(input_file), num_iter);
  std::sort(expected.begin(), expected.end());

  REQUIRE(expected == observed);
}

static auto kernel_fltmapjoin = [](KV& in1, KV& in2,
                                   pico::FlatMapCollector<KV>& collector) {
  KV res = in1 + in2;
  int res_value = res.Value();
  int res_abs = std::abs(res_value);
  if (res_value % 2 == 0) {
    for (int i = 0; i < res_abs % 10; ++i) {
      collector.add(res);
    }
  }  // else filters out the pairs
};

void seq_flatmap_join(const KvMultiMap& partitions_1,
                      const KvMultiMap& partitions_2, KvMultiMap& res) {
  res.clear();

  for (auto& part : partitions_1) {
    char key = part.first;
    const std::unordered_multiset<int>* ptr_values_1 = &(part.second);
    if (partitions_2.count(key) == 1) {
      const std::unordered_multiset<int>* ptr_values_2 =
          &(partitions_2.at(key));
      for (auto in1 : *ptr_values_1) {
        for (auto in2 : *ptr_values_2) {  // join
          int sum = in1 + in2;
          if (sum % 2 == 0) {
            int sum_abs = std::abs(sum);
            for (int i = 0; i < sum_abs % 10; ++i) {
              res[key].insert(sum);
            }
          }
        }
      }
    }
  }
}

KvMultiMap seq_Iter_flatmap_join(const KvMultiMap& original_partitions, int num_iter) {
  KvMultiMap res;
  KvMultiMap& ptr_res = res;
  KvMultiMap helper = original_partitions;
  KvMultiMap &ptr_helper = helper, tmp;

  for (int i = 0; i < num_iter; ++i) {
    seq_flatmap_join(original_partitions, ptr_helper, ptr_res);
    tmp = ptr_helper;
    ptr_helper = ptr_res;
    ptr_res = tmp;
  }
  if (num_iter > 0) ptr_res = ptr_helper;

  return ptr_res;
}

TEST_CASE("iteration with JoinFlatMapByKey", "[iterationTag]") {
  std::string input_file = "./testdata/pairs_64.txt";
  std::string output_file = "output.txt";

  pico::WriteToDisk<KV> writer(output_file,
                               [](KV in) { return in.to_string(); });

  auto pairs = pipe_pairs_creator<KV>(input_file);

  const int num_iter = 3;
  auto iter_pipe =
      pico::Pipe()
          .pair_with(pairs,
                     pico::JoinFlatMapByKey<KV, KV, KV>(kernel_fltmapjoin))
          .iterate(pico::FixedIterations(num_iter));

  auto test_pipe = pairs.to(iter_pipe).add(writer);

  test_pipe.run();

  std::unordered_map<char, std::unordered_multiset<int>> partitions;
  auto input_pairs_str = read_lines(input_file);
  for (auto pair : input_pairs_str) {
    auto kv = KV::from_string(pair);
    partitions[kv.Key()].insert(kv.Value());
  }
  auto expected = seq_Iter_flatmap_join(partitions, num_iter);
  auto observed = result_fltmapjoin<KV>(output_file);

  REQUIRE(expected == observed);
}
