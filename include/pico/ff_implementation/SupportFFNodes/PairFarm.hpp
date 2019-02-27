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

#ifndef PICO_FF_IMPLEMENTATION_SUPPORTFFNODES_PAIRFARM_HPP_
#define PICO_FF_IMPLEMENTATION_SUPPORTFFNODES_PAIRFARM_HPP_

#include <ff/node.hpp>

#include "pico/Internals/utils.hpp"

#include "base_nodes.hpp"
#include "farms.hpp"

/*
 *******************************************************************************
 *
 * Emitters
 *
 *******************************************************************************
 */
typedef base_emitter PairEmitter_base_t;
class PairEmitterToNone : public PairEmitter_base_t {
 public:
  PairEmitterToNone() : PairEmitter_base_t(2) {}

  /* ensure no c-streams are sent to input-less pipes */
  void kernel(pico::base_microbatch *) { assert(false); }

  virtual void handle_cstream_begin(pico::base_microbatch::tag_t tag) {
    assert(false);
  }

  virtual void handle_cstream_end(pico::base_microbatch::tag_t tag) {
    assert(false);
  }
};

template <int To>
class PairEmitterTo : public PairEmitter_base_t {
 public:
  PairEmitterTo() : PairEmitter_base_t(2) {}

  void kernel(pico::base_microbatch *in_mb) { send_mb_to(in_mb, To); }

  /* do not notify input-less pipe about c-stream begin/end */
  virtual void handle_cstream_begin(pico::base_microbatch::tag_t tag) {
    send_mb_to(make_sync(tag, PICO_CSTREAM_BEGIN), To);
    // cstream_begin_callback(tag);
  }

  virtual void handle_cstream_end(pico::base_microbatch::tag_t tag) {
    // cstream_end_callback(tag);
    send_mb_to(make_sync(tag, PICO_CSTREAM_END), To);
  }
};

using PairEmitterToFirst = PairEmitterTo<0>;
using PairEmitterToSecond = PairEmitterTo<1>;

/*
 *******************************************************************************
 *
 * Collector-side: origin-tracking gatherer + origin-decorating collector
 *
 *******************************************************************************
 */
class PairGatherer : public ff::ff_gatherer {
 public:
  explicit PairGatherer(size_t n) : ff::ff_gatherer(n), from_(-1) {}

  ssize_t from() const { return from_; }

 private:
  ssize_t selectworker() {
    from_ = ff::ff_gatherer::selectworker();
    return from_;
  }

  ssize_t from_;
};

class PairCollector : public base_sync_duplicate {
 public:
  explicit PairCollector(PairGatherer &gt_) : base_sync_duplicate(2), gt(gt_) {}

  /* on c-stream begin, forward and notify about origin */
  virtual void handle_cstream_begin(pico::base_microbatch::tag_t tag) {
    send_mb(make_sync(tag, PICO_CSTREAM_BEGIN));
    if (!gt.from())
      send_mb(make_sync(tag, PICO_CSTREAM_FROM_LEFT));
    else
      send_mb(make_sync(tag, PICO_CSTREAM_FROM_RIGHT));
  }

  /* on c-stream end, notify */
  virtual void handle_cstream_end(pico::base_microbatch::tag_t tag) {
    // cstream_end_callback(tag);
    send_mb(make_sync(tag, PICO_CSTREAM_END));
  }

  /* forward data */
  void kernel(pico::base_microbatch *in_mb) { send_mb(in_mb); }

 private:
  PairGatherer &gt;
};

class PairFarm : public ff::ff_farm {
 public:
  PairFarm() {
    setlb(new NonOrderingFarm::lb_t(max_nworkers));  // memory leak?
    setgt(new PairGatherer(max_nworkers));
  }
};

static ff::ff_pipeline *make_ff_pipe(const pico::Pipe &p1,
                                     pico::StructureType st, bool);  // forward

/*
 * return nullptr in case of error
 */

static PairFarm *make_pair_farm(const pico::Pipe &p1, const pico::Pipe &p2,  //
                                pico::StructureType st) {
  /* create and configure */
  auto res = new PairFarm();
  res->cleanup_all();

  /* add emitter */
  ff::ff_node *e;
  if (p1.in_deg())
    e = new PairEmitterToFirst();
  else if (p2.in_deg())
    e = new PairEmitterToSecond();
  else
    e = new PairEmitterToNone();
  res->add_emitter(e);

  /* add argument pipelines as workers */
  std::vector<ff::ff_node *> w;
  auto* sub_p1 = make_ff_pipe(p1, st, false);
  if (sub_p1) {
	  w.push_back(sub_p1);
  	  auto* sub_p2 = make_ff_pipe(p2, st, false);

  	  if (sub_p2) {
  		  w.push_back(sub_p2);
  		  res->add_workers(w);

  		  /* add collector */
  		  auto p_pair_gt = reinterpret_cast<PairGatherer *>(res->getgt());
  		  res->add_collector(new PairCollector(*p_pair_gt));
  	  }
  	  else {
  		  delete sub_p1;
  		  delete res;
  		  res = nullptr;
  	  }

  }
  else {
	  delete res;
	  res = nullptr;
  }

  return res;
}
#endif /* PICO_FF_IMPLEMENTATION_SUPPORTFFNODES_PAIRFARM_HPP_ */
