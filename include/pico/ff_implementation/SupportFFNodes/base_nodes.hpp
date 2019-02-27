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

#ifndef PICO_FF_IMPLEMENTATION_BASE_NODES_HPP_
#define PICO_FF_IMPLEMENTATION_BASE_NODES_HPP_

#ifdef TRACE_PICO
#include <chrono>
#endif

#include <ff/multinode.hpp>
#include <ff/node.hpp>

#include "pico/Internals/Microbatch.hpp"
#include "pico/Internals/utils.hpp"

#include "pico/ff_implementation/defs.hpp"

#include "farms.hpp"

using base_node = ff::ff_node_t<pico::base_microbatch, pico::base_microbatch>;
using base_monode =
    ff::ff_monode_t<pico::base_microbatch, pico::base_microbatch>;

static pico::base_microbatch *make_sync(pico::base_microbatch::tag_t tag,
                                        char *token) {
  return NEW<pico::base_microbatch>(tag, token);
}

class sync_handler {
 public:
  virtual ~sync_handler() {}

 protected:
  /*
   * to be overridden by user code
   */
  virtual void kernel(pico::base_microbatch *) = 0;

  /* default behaviors cannot be given for routing sync tokens */
  virtual void handle_begin(pico::base_microbatch::tag_t tag) = 0;
  virtual void handle_end(pico::base_microbatch::tag_t tag) = 0;
  virtual void handle_cstream_begin(pico::base_microbatch::tag_t tag) = 0;
  virtual void handle_cstream_end(pico::base_microbatch::tag_t tag) = 0;

  virtual void handle_sync(pico::base_microbatch *sync_mb) {
    char *token = sync_mb->payload();
    auto tag = sync_mb->tag();
    if (token == PICO_BEGIN)
      handle_begin(tag);
    else if (token == PICO_END)
      handle_end(tag);
    else if (token == PICO_CSTREAM_BEGIN)
      handle_cstream_begin(tag);
    else if (token == PICO_CSTREAM_END)
      handle_cstream_end(tag);
  }
};

class sync_handler_filter : public sync_handler {
 public:
  virtual ~sync_handler_filter() {}

 protected:
  virtual void begin_callback() {}

  virtual void end_callback() {}

  virtual void cstream_begin_callback(pico::base_microbatch::tag_t) {}

  virtual void cstream_end_callback(pico::base_microbatch::tag_t) {}

  virtual bool propagate_cstream_sync() { return true; }

  /*
   * to be called by user code and runtime
   */
  void begin_cstream(pico::base_microbatch::tag_t tag) {
    send_mb(make_sync(tag, PICO_CSTREAM_BEGIN));
  }

  void end_cstream(pico::base_microbatch::tag_t tag) {
    send_mb(make_sync(tag, PICO_CSTREAM_END));
  }

  /*
   * to be overridden by user code
   */

  virtual void send_mb(pico::base_microbatch *sync_mb) = 0;

  void work_flow(pico::base_microbatch *in) {
#ifdef TRACE_PICO
    auto t0 = std::chrono::high_resolution_clock::now();
#endif
    if (!is_sync(in->payload())) {
#ifdef TRACE_PICO
      tag_cnt[in->tag()].rcvd_data++;
#endif
      kernel(in);
    } else {
#ifdef TRACE_PICO
      tag_cnt[in->tag()].rcvd_sync++;
#endif
      handle_sync(in);
      DELETE(in);
    }
#ifdef TRACE_PICO
    auto t1 = std::chrono::high_resolution_clock::now();
    svcd += (t1 - t0);
#endif
  }

 private:
  virtual void handle_begin(pico::base_microbatch::tag_t tag) {
    // fprintf(stderr, "> %p begin tag=%llu\n", this, tag);
    assert(tag == pico::base_microbatch::nil_tag());
    send_mb(make_sync(tag, PICO_BEGIN));
    begin_callback();
  }

  virtual void handle_end(pico::base_microbatch::tag_t tag) {
    // fprintf(stderr, "> %p end tag=%llu\n", this, tag);
    assert(tag == pico::base_microbatch::nil_tag());
    end_callback();
    send_mb(make_sync(tag, PICO_END));
  }

  virtual void handle_cstream_begin(pico::base_microbatch::tag_t tag) {
    // fprintf(stderr, "> %p c-begin tag=%llu\n", this, tag);
    if (propagate_cstream_sync()) begin_cstream(tag);
    cstream_begin_callback(tag);
  }

  virtual void handle_cstream_end(pico::base_microbatch::tag_t tag) {
    // fprintf(stderr, "> %p c-end tag=%llu\n", this, tag);
    cstream_end_callback(tag);
    if (propagate_cstream_sync()) end_cstream(tag);
  }

#ifdef TRACE_PICO
  std::chrono::duration<double> svcd{0};
  struct mb_cnt {
    unsigned long long sent_data = 0, sent_sync = 0;
    unsigned long long rcvd_data = 0, rcvd_sync = 0;
  };
  std::unordered_map<pico::base_microbatch::tag_t, mb_cnt> tag_cnt;

 protected:
  void print_mb_cnt(std::ostream &os) {
    unsigned long long total = 0;
    os << "  PICO-mb sent:\n";
    for (auto &mbc : tag_cnt) {
      os << "  [tag=" << mbc.first << "] sync=" << mbc.second.sent_sync;
      os << " data=" << mbc.second.sent_data << "\n";
      total += mbc.second.sent_data + mbc.second.sent_sync;
    }
    os << "  > sent-total=" << total << "\n";
    total = 0;
    os << "  PICO-mb received:\n";
    for (auto &mbc : tag_cnt) {
      os << "  [tag=" << mbc.first << "] sync=" << mbc.second.rcvd_sync;
      os << " data=" << mbc.second.rcvd_data << "\n";
      total += mbc.second.rcvd_data + mbc.second.rcvd_sync;
    }
    os << "  > rcvd-total=" << total << "\n";
  }

 private:
  void ffStats(std::ostream &os) {
    base_node::ffStats(os);
    os << "  PICO-svc ms   : " << svcd.count() * 1024 << "\n";
    print_mb_cnt(os);
  }
#endif
};

class base_filter : public base_node, public sync_handler_filter {
 public:
  virtual ~base_filter() {}

 protected:
  virtual void send_mb(pico::base_microbatch *sync_mb) {
    ff_send_out(sync_mb);
#ifdef TRACE_PICO
    if (!is_sync(sync_mb->payload()))
      tag_cnt[sync_mb->tag()].sent_data++;
    else
      tag_cnt[sync_mb->tag()].sent_sync++;
#endif
  }

 private:
  pico::base_microbatch *svc(pico::base_microbatch *in) {
    work_flow(in);
    return GO_ON;
  }
};

class base_emitter : public base_monode, public sync_handler_filter {
 public:
  explicit base_emitter(unsigned nw_) : nw(nw_) {}

  virtual ~base_emitter() {}

 protected:
  void send_mb_to(pico::base_microbatch *task, unsigned i) {
    ff_send_out_to(task, i);
  }

  void send_mb(pico::base_microbatch *sync_mb) {
    for (unsigned i = 0; i < nw; ++i)
      send_mb_to(make_sync(sync_mb->tag(), sync_mb->payload()), i);
  }

 private:
  unsigned nw;

  pico::base_microbatch *svc(pico::base_microbatch *in) {
    work_flow(in);
    return GO_ON;
  }

#ifdef TRACE_PICO
  std::chrono::duration<double> svcd;

  void ffStats(std::ostream &os) {
    base_node::ffStats(os);
    os << "  PICO-svc ms   : " << svcd.count() * 1024 << "\n";
  }
#endif
};

class base_ord_emitter : public base_filter {
 public:
  explicit base_ord_emitter(unsigned nw_) : nw(nw_) {}

  virtual ~base_ord_emitter() {}

  /*
   * works only with strict round-robin load balancer (e.g. ordered farm)
   */

  void send_mb(pico::base_microbatch *sync_mb) {
    for (unsigned i = 0; i < nw; ++i)
      ff_send_out(make_sync(sync_mb->tag(), sync_mb->payload()));
  }

 private:
  unsigned nw;

#ifdef TRACE_PICO
  std::chrono::duration<double> svcd;

  void ffStats(std::ostream &os) {
    base_node::ffStats(os);
    os << "  PICO-svc ms   : " << svcd.count() * 1024 << "\n";
  }
#endif
};

class base_sync_duplicate : public base_filter {
 public:
  explicit base_sync_duplicate(unsigned nw_) : nw(nw_) {
    pending_begin = pending_end = nw;
  }

  virtual ~base_sync_duplicate() {}

 private:
  void handle_end(pico::base_microbatch::tag_t tag) {
    assert(tag == pico::base_microbatch::nil_tag());
    assert(pending_begin < pending_end);
    if (!--pending_end) send_mb(make_sync(tag, PICO_END));
    assert(pending_begin <= pending_end);
  }

  void handle_begin(pico::base_microbatch::tag_t tag) {
    assert(tag == pico::base_microbatch::nil_tag());
    assert(pending_begin <= pending_end);
    if (!--pending_begin) send_mb(make_sync(tag, PICO_BEGIN));
    assert(pending_begin < pending_end);
  }

  virtual void handle_cstream_end(pico::base_microbatch::tag_t tag) {
    assert(pending_cstream_begin.find(tag) != pending_cstream_begin.end());
    if (pending_cstream_end.find(tag) == pending_cstream_end.end())
      pending_cstream_end[tag] = nw;
    assert(pending_cstream_begin[tag] < pending_cstream_end[tag]);
    if (!--pending_cstream_end[tag]) {
      this->cstream_end_callback(tag);
      if (propagate_cstream_sync()) send_mb(make_sync(tag, PICO_CSTREAM_END));
    }
  }

  virtual void handle_cstream_begin(pico::base_microbatch::tag_t tag) {
    if (pending_cstream_begin.find(tag) == pending_cstream_begin.end()) {
      if (propagate_cstream_sync()) send_mb(make_sync(tag, PICO_CSTREAM_BEGIN));
      pending_cstream_begin[tag] = nw;
    }
    if (pending_cstream_end.find(tag) != pending_cstream_end.end())
      assert(pending_cstream_begin[tag] <= pending_cstream_end[tag]);
    --pending_cstream_begin[tag];
  }

 private:
  unsigned nw;
  std::unordered_map<pico::base_microbatch::tag_t, unsigned>
      pending_cstream_begin;
  std::unordered_map<pico::base_microbatch::tag_t, unsigned>
      pending_cstream_end;
  unsigned pending_end, pending_begin;
};

class base_switch : public base_monode, public sync_handler {
 public:
  virtual ~base_switch() {}

 protected:
  void send_mb_to(pico::base_microbatch *task, unsigned dst) {
    this->ff_send_out_to(task, dst);
  }

 private:
  pico::base_microbatch *svc(pico::base_microbatch *in) {
    if (!is_sync(in->payload()))
      kernel(in);
    else {
      handle_sync(in);
      DELETE(in);
    }

    return GO_ON;
  }
};

class base_mplex
    : public ff::ff_minode_t<pico::base_microbatch, pico::base_microbatch> {
  typedef ff::ff_minode_t<pico::base_microbatch, pico::base_microbatch> base_t;

 public:
  virtual ~base_mplex() {}

 protected:
  /*
   * to be overridden by user code
   */
  virtual void kernel(pico::base_microbatch *) = 0;

  /* default behaviors cannot be given for routing sync tokens */
  virtual void handle_begin(pico::base_microbatch::tag_t tag) = 0;
  virtual bool handle_end(pico::base_microbatch::tag_t tag) = 0;
  virtual void handle_cstream_begin(pico::base_microbatch::tag_t tag) = 0;
  virtual void handle_cstream_end(pico::base_microbatch::tag_t tag) = 0;

  void send_mb(pico::base_microbatch *task) { this->ff_send_out(task); }

  unsigned from() { return base_t::get_channel_id(); }

 private:
  bool handle_sync(pico::base_microbatch *sync_mb) {
    char *token = sync_mb->payload();
    auto tag = sync_mb->tag();
    if (token == PICO_BEGIN)
      handle_begin(tag);
    else if (token == PICO_END)
      return handle_end(tag);
    else if (token == PICO_CSTREAM_BEGIN)
      handle_cstream_begin(tag);
    else if (token == PICO_CSTREAM_END)
      handle_cstream_end(tag);
    return false;
  }

  pico::base_microbatch *svc(pico::base_microbatch *in) {
    if (!is_sync(in->payload()))
      kernel(in);
    else {
      bool stop = handle_sync(in);
      DELETE(in);
      if (stop) return EOS;
    }

    return GO_ON;
  }
};

#endif /* PICO_FF_IMPLEMENTATION_BASE_NODES_HPP_ */
