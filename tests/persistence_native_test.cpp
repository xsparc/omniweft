// SPDX-License-Identifier: Apache-2.0
#include "omniweft/persistence.hpp"
#include "../src/persistence_io.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
namespace p=ow::persistence;
using namespace ow::commands;
unsigned checks=0;
struct TestFailure:std::runtime_error{using std::runtime_error::runtime_error;};
void require(bool value,const char* label){++checks;if(!value)throw TestFailure(label);}
struct Scratch {
  std::filesystem::path path;
  Scratch() {
    const auto base=std::filesystem::temp_directory_path();
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    for(unsigned attempt=0;attempt<100;++attempt) {
      const auto candidate=base/("ow-persistence-native-"+std::to_string(stamp)+"-"+std::to_string(attempt));
      if(std::filesystem::create_directory(candidate)){path=candidate;return;}
    }
    throw std::runtime_error("fresh scratch allocation");
  }
  ~Scratch(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
std::string hex(std::span<const std::uint8_t> bytes) {
  std::string text;constexpr char digits[]="0123456789abcdef";
  for(const auto byte:bytes){text+=digits[byte>>4];text+=digits[byte&15];}return text;
}
EntityTarget identity(std::uint64_t generation=1){return {"workshop","00000007-0000-4000-8000-000000000001",generation};}
Envelope request(const p::Store& store,std::uint64_t sequence,std::vector<Operation> operations) {
  Envelope e;e.world_id="workshop";e.transaction_id="018f7242-4387-7c98-a114-00000014000"+std::to_string(sequence);
  e.idempotency={store.epoch(),sequence};e.expected_world_revision=sequence-1;e.apply_at={"next_tick",120};e.budget={4,0};e.operations=std::move(operations);return e;
}
Envelope create(const p::Store& store){return request(store,1,{EntityCreate{"A","builtin.unit_cube"},TransformSet{TemporaryTarget{"A"},{-2,0,0}}});}
Envelope replace(const p::Store& store){return request(store,2,{EntityDelete{identity()},EntityCreate{"B","builtin.unit_cube"},TransformSet{TemporaryTarget{"B"},{4,0,0}}});}
void good(const p::Result& result,std::uint64_t sequence) {
  require(result.code.empty() && !result.recovery_required && result.receipt.has_value(),"durable receipt exists");
  require(result.receipt->status=="committed" && result.receipt->durability=="durable" && result.receipt->world_revision==sequence,"durable receipt identity");
  require(result.high_water>=sequence && result.durable_revision==result.high_water,"durable watermarks");
}
template<class F>void failure(F callback,const char* code) {
  bool failed=false;try{callback();}catch(const p::Failure& error){failed=std::string_view(error.code())==code;}
  require(failed,"fixed failure code");
}
p::io::Bytes read(const std::filesystem::path& path){return p::io::read(path,8U*1024U*1024U);}
void write(const std::filesystem::path& path,const p::io::Bytes& bytes){p::io::File file(path,false);file.write(bytes);file.flush();}
void hash_vectors() {
  require(hex(p::io::sha256({}))=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 empty known answer");
  const std::string abc="abc",longer="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const auto hash=[](const std::string& s){return hex(p::io::sha256({reinterpret_cast<const std::uint8_t*>(s.data()),s.size()}));};
  require(hash(abc)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc known answer");
  require(hash(longer)=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA256 multiblock known answer");
}
void history(const std::filesystem::path& path) {
  auto store=p::Store::create(path);require(store->high_water()==0 && store->low_water()==1,"empty persisted watermarks");
  const auto epoch=store->epoch();require(epoch.size()==64,"private epoch shape");failure([&]{p::Store::open(path);},"STORE_LOCKED");
  const auto first=create(*store);const auto initial=store->snapshot();
  auto wrong=first;wrong.idempotency.epoch=std::string(64,'0');
  require(store->apply_await_durable(wrong,1).code=="EPOCH_MISMATCH" && store->snapshot()==initial,"wrong epoch unchanged");
  wrong=first;wrong.idempotency.sequence=2;
  require(store->apply_await_durable(wrong,1).code=="SEQUENCE_GAP" && store->snapshot()==initial,"sequence gap unchanged");
  auto invalid=first;std::get<TransformSet>(invalid.operations[1]).scale[0]=0;
  require(store->apply_await_durable(invalid,1).code=="INVALID_SCHEMA" && store->high_water()==0,"rejection does not consume key");
  const auto committed=store->apply_await_durable(first,1);good(committed,1);const auto one=store->snapshot();
  const auto bytes=read(path/"journal.bin");const auto repeated=store->apply_await_durable(first,9);
  good(repeated,1);require(repeated.replayed && repeated.receipt==committed.receipt && read(path/"journal.bin")==bytes && store->snapshot()==one,"exact retry no write");
  wrong=first;std::get<TransformSet>(wrong.operations[1]).position_m[0]=5;
  require(store->apply_await_durable(wrong,2).code=="IDEMPOTENCY_CONFLICT" && store->snapshot()==one,"altered retry rejected");
  store.reset();store=p::Store::open(path);require(store->epoch()==epoch && store->snapshot()==one,"recovery preserves private epoch and state");
  good(store->apply_await_durable(replace(*store),2),2);
  auto two=store->snapshot();require(two.world_revision==2 && two.slots.size()==1 && two.slots[0].generation==2 &&
    two.slots[0].entity->transform.position_m==std::array<double,3>{4,0,0},"delete reuse stable UUID next generation");
  require(store->compact().code.empty() && read(path/"journal.bin").empty(),"checkpoint before reclaim");
  store.reset();store=p::Store::open(path);require(store->snapshot()==two,"checkpoint replay exact");
  for(std::uint64_t i=3;i<=8;++i) {
    good(store->apply_await_durable(request(*store,i,{TransformSet{identity(2),{static_cast<double>(i),0,0}}}),i),i);
    if(i==4){require(store->compact().code.empty(),"second compaction");store.reset();store=p::Store::open(path);}
  }
  require(store->low_water()==5 && store->high_water()==8,"four receipt retention bound");
  require(store->lookup(4).code=="REQUIRES_RESYNC" && store->lookup(9).code=="NOT_FOUND","lookup outside retained window");
  for(std::uint64_t i=5;i<=8;++i)good(store->lookup(i),i);
  require(store->apply_await_durable(first,10).code=="REQUIRES_RESYNC","evicted key cannot reexecute");
  const auto full=store->snapshot();
  require(store->apply_await_durable(request(*store,9,{TransformSet{identity(2),{9,0,0}}}),9).code=="BUDGET_EXCEEDED" && store->snapshot()==full,"total commit cap definite rejection");
  store.reset();store=p::Store::open(path);require(store->snapshot()==full && store->low_water()==5,"retained receipt window recovers");
}
void sealing(const std::filesystem::path& base) {
  const std::array phases{p::Phase::before_blob_flush,p::Phase::before_journal_flush,p::Phase::after_journal_flush,p::Phase::before_ack};
  unsigned number=0;
  for(const auto phase:phases) {
    const auto path=base/("seal-"+std::to_string(number++));auto store=p::Store::create(path);good(store->apply_await_durable(create(*store),1),1);store.reset();
    store=p::Store::open(path,[=](p::Phase current){if(current==phase)throw std::runtime_error("injected I/O uncertainty");});
    const auto edit=replace(*store);const auto result=store->apply_await_durable(edit,2);
    require(result.code=="OUTCOME_UNKNOWN" && result.recovery_required && store->recovery_required(),"uncertainty seals writer");
    require(store->apply_await_durable(edit,2).code=="RECOVERY_REQUIRED" && store->compact().code=="RECOVERY_REQUIRED" && store->lookup(1).code=="RECOVERY_REQUIRED","sealed calls cannot proceed");
    failure([&]{store->snapshot();},"RECOVERY_REQUIRED");store.reset();store=p::Store::open(path);
    const auto revision=phase==p::Phase::before_blob_flush?1U:2U;
    require(store->high_water()==revision,"recovery selects complete frame only");
    const auto retry=store->apply_await_durable(edit,2);good(retry,2);require(retry.replayed==(revision==2),"unknown result discovered or applied once");
  }
}
void corruption(const std::filesystem::path& path) {
  auto store=p::Store::create(path);good(store->apply_await_durable(create(*store),1),1);const auto one=store->snapshot();
  good(store->apply_await_durable(replace(*store),2),2);store.reset();const auto original=read(path/"journal.bin");
  const auto payload_size=[](const p::io::Bytes& data,std::size_t at) {
    std::uint32_t size=0;for(unsigned i=0;i<4;++i)size|=std::uint32_t(data[at+8+i])<<(8*i);return size;
  };
  const auto second=static_cast<std::size_t>(payload_size(original,0))+92;
  require(second<original.size(),"independently locate acknowledged final frame");
  for(const auto offset:std::array<std::size_t,4>{7,9,12,20}) {
    auto corrupt_header=original;
    // Byte9 toggles length by16384: still within the256KiB budget, but beyond
    // remaining bytes. The unauthenticated-length implementation lost seq2.
    corrupt_header[second+offset]^=offset==9?0x40:0x01;
    if(offset==9)require(payload_size(corrupt_header,second)<=256U*1024U &&
      payload_size(corrupt_header,second)>corrupt_header.size()-second,"length corruption stays bounded but exceeds remaining bytes");
    write(path/"journal.bin",corrupt_header);
    failure([&]{p::Store::open(path);},"CORRUPT_STORE");
    require(read(path/"journal.bin")==corrupt_header,"corrupt complete framing never truncates durable key");
  }
  auto bad=original;bad[55]^=1;write(path/"journal.bin",bad);failure([&]{p::Store::open(path);},"CORRUPT_STORE");
  require(read(path/"journal.bin")==bad,"complete corrupt frame preserved fail closed");
  bad=original;bad.pop_back();write(path/"journal.bin",bad);store=p::Store::open(path);
  require(store->snapshot()==one && read(path/"journal.bin").size()<bad.size(),"incomplete last frame repairs to valid prefix");
  good(store->apply_await_durable(replace(*store),2),2);require(store->compact().code.empty(),"recovered append remains compactable");store.reset();
  const auto checkpoint=read(path/"checkpoint.bin");bad=checkpoint;bad[0]^=1;write(path/"checkpoint.bin",bad);
  failure([&]{p::Store::open(path);},"CORRUPT_STORE");require(read(path/"checkpoint.bin")==bad,"corrupt selected checkpoint never rolls back");
  write(path/"checkpoint.bin",checkpoint);store=p::Store::open(path);require(store->high_water()==2,"restore original package recovers");
}
void quotas(const std::filesystem::path& path) {
  // The fixed 2KiB quota fits initialization but cannot fit the first full frame.
  p::Config config;config.byte_quota=2048;auto store=p::Store::create(path,config);
  const auto snapshot=store->snapshot();const auto journal=read(path/"journal.bin"),checkpoint=read(path/"checkpoint.bin");
  const auto result=store->apply_await_durable(create(*store),1);
  require(result.code=="QUOTA_EXCEEDED" && !result.recovery_required && store->snapshot()==snapshot && store->high_water()==0,"quota definite rejection before world publication");
  require(read(path/"journal.bin")==journal && read(path/"checkpoint.bin")==checkpoint,"quota leaves package bytes unchanged");
}
void compaction_failures(const std::filesystem::path& base) {
  const std::array phases{p::Phase::before_checkpoint_flush,p::Phase::after_checkpoint_flush,
    p::Phase::before_checkpoint_replace,p::Phase::after_checkpoint_replace,
    p::Phase::before_journal_reclaim,p::Phase::after_journal_reclaim};
  unsigned number=0;
  for(const auto phase:phases) {
    const auto path=base/("compact-"+std::to_string(number++));auto store=p::Store::create(path);
    good(store->apply_await_durable(create(*store),1),1);good(store->apply_await_durable(replace(*store),2),2);
    for(std::uint64_t i=3;i<=5;++i)good(store->apply_await_durable(request(*store,i,{TransformSet{identity(2),{static_cast<double>(i),0,0}}}),i),i);
    const auto snapshot=store->snapshot();const auto epoch=store->epoch();store.reset();
    store=p::Store::open(path,[=](p::Phase current){if(current==phase)throw std::runtime_error("injected checkpoint uncertainty");});
    require(store->compact().code=="OUTCOME_UNKNOWN" && store->recovery_required(),"compaction uncertainty seals writer");
    store.reset();store=p::Store::open(path);
    require(store->snapshot()==snapshot && store->epoch()==epoch && store->high_water()==5 && store->low_water()==2,"interrupted compaction preserves complete acknowledged window");
    require(store->lookup(1).code=="REQUIRES_RESYNC","compaction cannot resurrect evicted receipts");good(store->lookup(2),2);
    require(store->compact().code.empty(),"compaction after reopen recovers");
  }
  const auto calibration=base/"quota-calibration";auto store=p::Store::create(calibration);good(store->apply_await_durable(create(*store),1),1);
  const auto frame_bytes=read(calibration/"journal.bin").size();const auto used=p::io::bytes_used(calibration);store.reset();
  p::Config config;config.byte_quota=used+frame_bytes-1;
  const auto path=base/"compact-quota";store=p::Store::create(path,config);good(store->apply_await_durable(create(*store),1),1);
  const auto before=store->snapshot();const auto journal=read(path/"journal.bin"),checkpoint=read(path/"checkpoint.bin");
  const auto result=store->compact();
  require(result.code=="QUOTA_EXCEEDED" && !result.recovery_required && store->snapshot()==before,"compaction reserves coexistence quota before writes");
  require(read(path/"journal.bin")==journal && read(path/"checkpoint.bin")==checkpoint && !std::filesystem::exists(path/"checkpoint.tmp"),"compaction quota preserves files");
}
}
int main(){try {
  Scratch scratch;hash_vectors();history(scratch.path/"history");sealing(scratch.path);corruption(scratch.path/"corrupt");quotas(scratch.path/"quota");compaction_failures(scratch.path);
  std::cout<<"{\"status\":\"passed\",\"case_set\":\"persistence-v1\",\"assertions\":"<<checks<<"}\n";return 0;
}catch(const TestFailure& error){std::cerr<<"persistence native test failed: "<<error.what()<<'\n';return 1;}
catch(...){std::cerr<<"persistence native test failed; private details withheld\n";return 1;}}
