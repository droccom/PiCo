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

#ifndef PICO_FASTFLOWEXECUTOR_HPP_
#define PICO_FASTFLOWEXECUTOR_HPP_

#include <fstream>
#include <string>

#include <ff/ff.hpp>

#include "pico/Operators/BinaryOperator.hpp"
#include "pico/Operators/UnaryOperator.hpp"
#include "pico/PEGOptimizations.hpp"
#include "pico/Pipe.hpp"

#include "SupportFFNodes/ForwardingNode.hpp"
#include "SupportFFNodes/PairFarm.hpp"
#include "SupportFFNodes/base_nodes.hpp"
#include "defs.hpp"

static ff::ff_pipeline *make_ff_pipe(const pico::Pipe &, pico::StructureType,
                                     bool);
static bool add_chain(ff::ff_pipeline *, const std::vector<pico::Pipe *> &,
                      pico::StructureType);
#if 0
static ff::ff_farm<> *make_merge_farm(const Pipe &);
static ff::ff_farm<> *make_multito_farm(const Pipe &);
#endif

/*
 * return true in case of success, false otherwise
 */

template <typename ItType>
bool add_plain(ff::ff_pipeline *p, ItType it, pico::StructureType st) {
  bool res = true;
  if ((*it)->term_node_type() == pico::Pipe::OPERATOR) {
    /* standalone operator */
    pico::base_UnaryOperator *op;
    op = dynamic_cast<pico::base_UnaryOperator *>((*it)->get_operator_ptr());
    p->add_stage(op->node_operator(op->pardeg(), st));
  } else {
    /* complex sub-term */
    auto *sub_pipe = make_ff_pipe(**it, st, false);
    if (sub_pipe)
      p->add_stage(sub_pipe);
    else
      res = false;
  }
  return res;
}

/*
 * return nullptr in case of error
 */

static ff::ff_pipeline *make_ff_pipe(const pico::Pipe &p,
                                     pico::StructureType st,  //
                                     bool acc) {
  /* create the ff pipeline with automatic node cleanup */
  auto *res = new ff::ff_pipeline(acc);
  ff::ff_pipeline *sub_pipe = nullptr;
  PairFarm *pair_farm = nullptr;

  res->cleanup_nodes();
  pico::Operator *op;
  pico::base_UnaryOperator *uop;
  pico::base_BinaryOperator *bop;
  pico::TerminationCondition *cond;

  switch (p.term_node_type()) {
    case pico::Pipe::EMPTY:
      res->add_stage(new ForwardingNode());
      break;
    case pico::Pipe::OPERATOR:
      op = p.get_operator_ptr();
      uop = dynamic_cast<pico::base_UnaryOperator *>(op);
      if (uop)
        res->add_stage(uop->node_operator(uop->pardeg(), st));
      else {
        delete res;
        res = nullptr;
        std::cerr
            << "FastFlowExecutor error in make_ff_pipe function. Case OPERATOR."
            << std::endl;
      }

      break;
    case pico::Pipe::TO:
      if (!add_chain(res, p.children(), st)) {
        delete res;
        res = nullptr;
      }
      break;
    case pico::Pipe::MULTITO:
      std::cerr << "MULTI-TO not implemented yet\n";
      assert(false);
      // res->add_stage(make_ff_pipe(*p.children().front(), false, par));
      // res->add_stage(make_multito_farm(p, par));
      break;
    case pico::Pipe::ITERATE:
      cond = p.get_termination_ptr();
      assert(p.children().size() == 1);
      res->add_stage(new iteration_multiplexer());
      sub_pipe = make_ff_pipe(*p.children().front(), st, false);
      if (sub_pipe) {
        res->add_stage(sub_pipe);
        res->add_stage(cond->iteration_switch());
        res->wrap_around();
      } else {
        delete res;
        res = nullptr;
      }
      break;
    case pico::Pipe::MERGE:
      std::cerr << "MERGE not implemented yet\n";
      assert(false);
      // res->add_stage(make_merge_farm(p, par));
      break;
    case pico::Pipe::PAIR:
      op = p.get_operator_ptr();
      assert(op);
      assert(p.children().size() == 2);
      pair_farm = make_pair_farm(*p.children()[0], *p.children()[1], st);
      if (pair_farm)
        res->add_stage(pair_farm);
      else {
        delete res;
        res = nullptr;
      }
      /* add the operator */
      bop = dynamic_cast<pico::base_BinaryOperator *>(op);
      if (bop) {
        bool left_input = p.children()[0]->in_deg();
        res->add_stage(bop->node_operator(bop->pardeg(), left_input, st));
      } else {
        delete res;
        res = nullptr;
        std::cerr
            << "FastFlowExecutor error in make_ff_pipe function. Case PAIR."
            << std::endl;
      }
      break;
  }
  return res;
}

#if 0
static ff::ff_farm<> *make_merge_farm(const Pipe &p) {
	/*
	 auto *res = new ff::ff_farm<>();
	 auto nw = p.children().size();
	 res->add_emitter(new BCastEmitter(nw, res->getlb()));
	 res->add_collector(new MergeCollector());
	 std::vector<ff::ff_node *> w;
	 for (auto p_ : p.children())
	 w.push_back(make_ff_term(*p_, false));
	 res->add_workers(w);
	 res->cleanup_all();
	 return res;
	 */
	assert(false);
	return nullptr;
}

ff::ff_farm<> *make_multito_farm(const Pipe &p) {
	/*
	 auto *res = new ff::ff_farm<>();
	 auto nw = p.children().size() - 1;
	 res->add_emitter(new BCastEmitter(nw, res->getlb()));
	 res->add_collector(new MergeCollector());
	 std::vector<ff::ff_node *> w;
	 for (auto it = p.children().begin() + 1; it != p.children().end(); ++it)
	 w.push_back(make_ff_term(**it, false));
	 res->add_workers(w);
	 res->cleanup_all();
	 return res;
	 */
	assert(false);
	return nullptr;
}
#endif

/*
 * return true in case of success, false otherwise
 */
bool add_chain(ff::ff_pipeline *p, const std::vector<pico::Pipe *> &s,  //
               pico::StructureType st) {
  /* apply PEG optimizations */
  bool res = true;
  auto it = s.begin();
  for (; (it < s.end() - 1) && res; ++it) {
    /* try to add an optimized compound */
    if (add_optimized(p, it, it + 1, st))
      ++it;
    else
      /* add a regular sub-term */
      res = add_plain(p, it, st);
  }
  /* add last sub-term if any */
  if (it != s.end() && res) res = add_plain(p, it, st);
  return res;
}

class FastFlowExecutor {
 public:
  explicit FastFlowExecutor(const pico::Pipe &p) {
    ff_pipe = make_ff_pipe(p, p.structure_type(), true);
  }

  ~FastFlowExecutor() { delete_ff_term(); }

  void run() const {
    if (!ff_pipe) {
      std::cerr
          << "error FastFlowExecutor, function \"run\". ff_pipe == nullptr"
          << std::endl;
    }
    auto tag = pico::base_microbatch::nil_tag();
    pico::base_microbatch *res;

    ff::OptLevel opt;
    opt.remove_collector = true;
    opt.verbose_level = 2;
    opt.max_nb_threads = ff_realNumCores();
    opt.max_mapped_threads = opt.max_nb_threads;
    opt.no_default_mapping = true;
    opt.blocking_mode = true;

    optimize_static(*ff_pipe, opt);

    ff_pipe->run();

    ff_pipe->offload(make_sync(tag, PICO_BEGIN));
    ff_pipe->offload(make_sync(tag, PICO_END));
    ff_pipe->offload(ff::FF_EOS);

    assert(ff_pipe->load_result((void **)&res));
    assert(res->payload() == PICO_BEGIN && res->tag() == tag);
    assert(ff_pipe->load_result((void **)&res));
    assert(res->payload() == PICO_END && res->tag() == tag);

    ff_pipe->wait();
  }

  double run_time() const { return ff_pipe->ffTime(); }

  void print_stats(std::ostream &os) const {
    if (ff_pipe) ff_pipe->ffStats(os);
  }

 private:
  // const Pipe &pipe;
  ff::ff_pipeline *ff_pipe = nullptr;

  void delete_ff_term() {
    if (ff_pipe)
      // ff recursively deletes the term (by node cleanup)
      delete ff_pipe;
  }
};

FastFlowExecutor *make_executor(const pico::Pipe &p) {
  auto mb_env = std::getenv("MBSIZE");
  if (mb_env) pico::global_params.MICROBATCH_SIZE = atoi(mb_env);

  return new FastFlowExecutor(p);
}

void destroy_executor(FastFlowExecutor *e) { delete e; }

void run_pipe(FastFlowExecutor &e) { e.run(); }

double run_time(FastFlowExecutor &e) { return e.run_time(); }

void print_executor_stats_(FastFlowExecutor &e, std::ostream &os) {
  e.print_stats(os);
}

#endif /* PICO_FASTFLOWEXECUTOR_HPP_ */
