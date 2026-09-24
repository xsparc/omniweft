// SPDX-License-Identifier: Apache-2.0
#include "omniweft/retry.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <limits>

namespace {
unsigned checks=0;
void check(bool v,const char* why) {++checks;if(!v) throw std::runtime_error(why);}
void denied(const ow::retry::Result& r,const char* code) {check(!r.receipt && r.code==std::string_view(code),"exact denial without receipt");}
}
int main() {
  try {
    using ow::retry::Window;
    Window window;unsigned calls=0;
    const auto apply=[&] {ow::transactions::Receipt r;r.status="committed";r.world_revision=++calls;return r;};
    denied(window.execute(0,"p",apply),"REQUIRES_RESYNC");
    denied(window.execute(2,"p",apply),"REQUIRES_RESYNC");
    denied(window.execute(std::numeric_limits<std::uint64_t>::max(),"p",apply),"REQUIRES_RESYNC");
    check(calls==0 && window.next_sequence()==1,"gaps do not consume");
    denied(window.execute(1,"",apply),"BUDGET_EXCEEDED");
    denied(window.execute(1,std::string(16385,'x'),apply),"BUDGET_EXCEEDED");
    denied(window.execute(1,"p",{}),"BUDGET_EXCEEDED");
    check(calls==0 && window.next_sequence()==1,"failed preparation does not consume");
    auto first=window.execute(1,"p",apply);check(first.receipt && !first.replayed && first.receipt->world_revision==1,"first admitted once");
    const auto saved=*first.receipt;
    for(unsigned i=0;i<32;++i) {
      const auto replay=window.execute(1,"p",apply);check(replay.receipt && replay.replayed && *replay.receipt==saved,"exact receipt replay");
      denied(window.execute(1,"changed",apply),"IDEMPOTENCY_MISMATCH");
    }
    check(calls==1 && window.next_sequence()==2,"replays and mismatch never apply or consume");
    for(std::uint64_t n=2;n<=5;++n) {
      const auto result=window.execute(n,std::string(16384,'x'),apply);
      check(result.receipt && result.receipt->world_revision==n,"exact payload cap and monotonic consumption");
    }
    denied(window.execute(1,"p",apply),"REQUIRES_RESYNC");
    check(calls==5 && window.next_sequence()==6,"compaction keeps admission high-water mark");
    const auto replay=window.execute(2,std::string(16384,'x'),apply);
    check(replay.receipt && replay.receipt->world_revision==2 && window.next_sequence()==6,"old receipt with current sequence");
    Window broken;
    unsigned failures=0;
    const auto throwing=[&]() -> ow::transactions::Receipt {++failures;throw std::runtime_error("callback");};
    denied(broken.execute(1,"p",throwing),"REQUIRES_RESYNC");
    denied(broken.execute(1,"p",throwing),"REQUIRES_RESYNC");
    denied(broken.execute(1,"other",throwing),"IDEMPOTENCY_MISMATCH");
    check(failures==1 && broken.next_sequence()==2,"callback exception consumes and never reexecutes");
    const auto oversized=[] {ow::transactions::Receipt r;r.errors.resize(5);return r;};
    denied(broken.execute(2,"p",oversized),"REQUIRES_RESYNC");
    denied(broken.execute(2,"p",apply),"REQUIRES_RESYNC");
    check(broken.next_sequence()==3,"oversized trusted result stays consumed");
    Window expiry;
    const auto original=expiry.execute(1,"p",apply);check(original.receipt!=nullptr,"expiry initial result");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    check(expiry.execute(1,"p",apply).replayed,"replay before absolute expiry");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    denied(expiry.execute(1,"p",apply),"REQUIRES_RESYNC");
    denied(expiry.execute(1,"changed",apply),"REQUIRES_RESYNC");
    check(expiry.next_sequence()==2 && expiry.execute(2,"new",apply).receipt,"expiry does not renew or reset sequence");
    expiry.reset();check(expiry.next_sequence()==1,"trusted epoch reset clears window");
    denied(expiry.execute(2,"new",apply),"REQUIRES_RESYNC");
    const auto after_reset=expiry.execute(1,"different-epoch-payload",apply);
    check(after_reset.receipt && !after_reset.replayed,"new epoch begins fresh only after trusted reset");
    std::cout<<"{\"status\":\"passed\",\"assertions\":"<<checks<<"}\n";return 0;
  } catch(const std::exception& error) {std::cerr<<"retry native failed: "<<error.what()<<'\n';return 1;}
}
