// SPDX-License-Identifier: Apache-2.0
#include "omniweft/persistence.hpp"
#include "omniweft/replay.hpp"
#include "persistence_io.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace ow::persistence {
namespace {
using Bytes=io::Bytes;
constexpr std::size_t package_limit=256U*1024U, header_size=52, frame_overhead=92;
constexpr std::size_t journal_limit=commit_limit*(package_limit+frame_overhead);
constexpr std::string_view frame_magic="OWJRN001",end_magic="OWEND001",package_magic="OWPKG001";
[[noreturn]] void corrupt(){throw Failure("CORRUPT_STORE");}
void check(bool value){if(!value)corrupt();}
struct Writer {
  Bytes bytes;
  void raw(std::span<const std::uint8_t> value){bytes.insert(bytes.end(),value.begin(),value.end());}
  void raw(std::string_view value){bytes.insert(bytes.end(),value.begin(),value.end());}
  void u64(std::uint64_t value){for(unsigned i=0;i<8;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
  void u32(std::uint32_t value){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
  void text(std::string_view value){u32(static_cast<std::uint32_t>(value.size()));raw(value);}
  void blob(const Bytes& value){u32(static_cast<std::uint32_t>(value.size()));raw(value);}
};
struct Reader {
  std::span<const std::uint8_t> bytes;std::size_t at=0;
  std::span<const std::uint8_t> raw(std::size_t size){check(size<=bytes.size()-at);auto v=bytes.subspan(at,size);at+=size;return v;}
  std::uint64_t integer(unsigned size){std::uint64_t n=0;const auto data=raw(size);for(unsigned i=0;i<size;++i)n|=std::uint64_t(data[i])<<(i*8);return n;}
  std::uint64_t u64(){return integer(8);}
  std::uint32_t u32(){return static_cast<std::uint32_t>(integer(4));}
  std::string text(std::size_t maximum){const auto n=u32();check(n<=maximum);const auto v=raw(n);return {v.begin(),v.end()};}
  Bytes blob(std::size_t maximum){const auto n=u32();check(n<=maximum);const auto v=raw(n);return {v.begin(),v.end()};}
  void magic(std::string_view value){const auto v=raw(value.size());check(std::equal(v.begin(),v.end(),value.begin()));}
};
struct Entry {
  std::uint64_t sequence=0,tick=0;
  std::string payload;
  std::vector<transactions::CreatedBinding> created;
  Bytes checkpoint;
  bool operator==(const Entry&) const=default;
};
struct Package {
  Config config;
  std::string epoch;
  std::vector<Entry> entries;
  std::vector<transactions::Receipt> receipts;
  std::uint64_t high() const noexcept{return entries.size();}
  std::uint64_t low() const noexcept{return high()>receipt_limit?high()-receipt_limit+1:1;}
};
bool same_config(const Config& a,const Config& b) {
  return a.world_id==b.world_id && a.seed==b.seed && a.max_slots==b.max_slots && a.byte_quota==b.byte_quota;
}
bool target_bounded(const commands::Target& target) {
  if(const auto* v=std::get_if<commands::EntityTarget>(&target))return v->world_id.size()<=128 && v->entity_uuid.size()<=36;
  return std::get<commands::TemporaryTarget>(target).temporary_id.size()<=64;
}
bool bounded(const commands::Envelope& e) {
  if(e.protocol_version.size()>16 || e.world_id.size()>128 || e.transaction_id.size()>36 ||
     e.idempotency.epoch.size()>128 || e.apply_at.mode.size()>32 || e.operations.empty() || e.operations.size()>operation_limit)return false;
  for(const auto& operation:e.operations) {
    const bool valid=std::visit([](const auto& op) {
      using T=std::decay_t<decltype(op)>;
      if constexpr(std::is_same_v<T,commands::EntityCreate>)return op.temporary_id.size()<=64 && op.prefab.size()<=128;
      else {
        if(!target_bounded(op.target))return false;
        if constexpr(std::is_same_v<T,commands::EntityTagsSet>) {
          if(op.tags.size()>8)return false;
          for(const auto& tag:op.tags)if(tag.size()>32)return false;
        } else if constexpr(std::is_same_v<T,commands::EntityReparent>) {
          if(op.mode.size()>32 || (op.parent && !target_bounded(*op.parent)))return false;
        } else if constexpr(std::is_same_v<T,commands::EntityDelete>) {if(op.child_policy.size()>32)return false;}
        return true;
      }
    },operation);
    if(!valid)return false;
  }
  return true;
}
void bindings(Writer& w,const std::vector<transactions::CreatedBinding>& values) {
  w.u32(static_cast<std::uint32_t>(values.size()));
  for(const auto& b:values){w.text(b.temporary_id);w.text(b.world_id);w.text(b.entity_uuid);w.u64(b.generation);}
}
std::vector<transactions::CreatedBinding> bindings(Reader& r) {
  const auto count=r.u32();check(count<=operation_limit);std::vector<transactions::CreatedBinding> result;result.reserve(count);
  for(std::uint32_t i=0;i<count;++i)result.push_back({r.text(64),r.text(128),r.text(36),r.u64()});
  return result;
}
// Payload v1: magic, config(world/seed/slots/quota), private epoch, high/low,
// pinned asset manifest/hash, full-prefix count and entries(seq/tick/canonical
// command/bindings/checkpoint), then last-four durable receipts. Every string
// and byte block is prefixed with u32 length. All integers are little-endian.
Bytes encode(const Package& p) {
  Writer w;w.raw(package_magic);w.text(p.config.world_id);w.u32(p.config.seed);w.u32(p.config.max_slots);w.u64(p.config.byte_quota);
  w.text(p.epoch);w.u64(p.high());w.u64(p.low());w.text(replay::builtin_manifest);w.text(replay::builtin_sha256);
  w.u32(static_cast<std::uint32_t>(p.entries.size()));
  for(const auto& e:p.entries){w.u64(e.sequence);w.u64(e.tick);w.text(e.payload);bindings(w,e.created);w.blob(e.checkpoint);}
  w.u32(static_cast<std::uint32_t>(p.receipts.size()));
  for(const auto& r:p.receipts) {
    w.text(r.status);w.text(r.durability);w.text(r.transaction_id);w.u64(r.world_revision);bindings(w,r.created);w.u32(0);
  }
  if(w.bytes.size()>package_limit)throw Failure("BUDGET_EXCEEDED");return std::move(w.bytes);
}
struct Restored {std::unique_ptr<Package> package;std::unique_ptr<world::World> world;};
Restored decode(std::span<const std::uint8_t> bytes) {
  Reader r{bytes};r.magic(package_magic);auto p=std::make_unique<Package>();
  p->config.world_id=r.text(128);p->config.seed=r.u32();p->config.max_slots=r.u32();p->config.byte_quota=r.u64();
  check(!p->config.world_id.empty() && p->config.max_slots>=1 && p->config.max_slots<=8 && p->config.byte_quota<=maximum_quota);
  p->epoch=r.text(64);check(p->epoch.size()==64 && std::all_of(p->epoch.begin(),p->epoch.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}));
  const auto high=r.u64(),low=r.u64();check(high<=commit_limit && low==(high>receipt_limit?high-receipt_limit+1:1));
  check(r.text(4096)==replay::builtin_manifest);check(r.text(64)==replay::builtin_sha256);
  const auto count=r.u32();check(count==high);p->entries.reserve(count);
  replay::Log log;log.world_id=p->config.world_id;log.seed=p->config.seed;log.max_slots=p->config.max_slots;
  std::vector<transactions::Receipt> expected_receipts;
  for(std::uint32_t i=0;i<count;++i) {
    Entry entry;entry.sequence=r.u64();entry.tick=r.u64();entry.payload=r.text(replay::command_limit);
    entry.created=bindings(r);entry.checkpoint=r.blob(replay::checkpoint_limit);
    check(entry.sequence==i+1 && (i==0 || entry.tick>=p->entries.back().tick));
    const auto parsed=commands::parse(entry.payload);check(parsed.envelope.has_value());const auto& e=*parsed.envelope;
    check(bounded(e) && e.world_id==p->config.world_id && e.expected_world_revision==i && e.idempotency.sequence==i+1 && e.idempotency.epoch==p->epoch);
    const auto serialized=commands::serialize(e);check(serialized.json && *serialized.json==entry.payload);
    log.records.push_back({entry.sequence,entry.tick,i,e.budget,e.operations,entry.created,entry.checkpoint});
    if(entry.sequence>=low) {
      transactions::Receipt receipt;receipt.status="committed";receipt.durability="durable";
      receipt.transaction_id=e.transaction_id;receipt.world_revision=entry.sequence;receipt.created=entry.created;
      expected_receipts.push_back(std::move(receipt));
    }
    p->entries.push_back(std::move(entry));
  }
  const auto retained=r.u32();check(retained==expected_receipts.size());p->receipts.reserve(retained);
  for(std::uint32_t i=0;i<retained;++i) {
    transactions::Receipt receipt;receipt.status=r.text(16);receipt.durability=r.text(16);receipt.transaction_id=r.text(36);
    receipt.world_revision=r.u64();receipt.created=bindings(r);check(r.u32()==0);check(receipt==expected_receipts[i]);p->receipts.push_back(std::move(receipt));
  }
  check(r.at==bytes.size());
  const std::array<replay::Asset,1> assets{replay::Asset{}};auto result=replay::reconstruct(log,assets);
  check(result.world && result.errors.empty());return {std::move(p),std::move(result.world)};
}
Bytes frame(const Package& package) {
  const auto payload=encode(package);Writer w;w.raw(frame_magic);w.u32(static_cast<std::uint32_t>(payload.size()));w.u64(package.high());
  // Authenticate framing separately: corrupted lengths must never turn a
  // complete acknowledged frame into an apparently incomplete tail.
  const auto header_digest=io::sha256(w.bytes);w.raw(header_digest);w.raw(payload);
  const auto digest=io::sha256(w.bytes);w.raw(digest);w.raw(end_magic);return std::move(w.bytes);
}
struct Frame {std::span<const std::uint8_t> payload;std::uint64_t high=0;};
std::optional<Frame> read_frame(std::span<const std::uint8_t> bytes,std::size_t& at) {
  const auto remaining=bytes.subspan(at);
  if(remaining.size()<header_size) {
    check(std::equal(remaining.begin(),remaining.begin()+static_cast<std::ptrdiff_t>(std::min<std::size_t>(remaining.size(),8)),frame_magic.begin()));
    if(remaining.size()>20) {
      const auto expected=io::sha256(remaining.first(20));
      check(std::equal(remaining.begin()+20,remaining.end(),expected.begin()));
    }
    return {};
  }
  const auto header_digest=io::sha256(remaining.first(20));
  check(std::equal(remaining.begin()+20,remaining.begin()+header_size,header_digest.begin()));
  Reader r{remaining};r.magic(frame_magic);const auto size=r.u32();check(size<=package_limit);const auto high=r.u64();check(high<=commit_limit);
  static_cast<void>(r.raw(32));
  if(remaining.size()<size+frame_overhead)return {};
  const auto payload=r.raw(size),digest=r.raw(32);const auto expected=io::sha256(remaining.first(header_size+size));
  check(std::equal(digest.begin(),digest.end(),expected.begin()));r.magic(end_magic);at+=r.at;return Frame{payload,high};
}
void prefix(const Package& earlier,const Package& later) {
  check(same_config(earlier.config,later.config) && earlier.epoch==later.epoch && earlier.high()<=later.high());
  check(std::equal(earlier.entries.begin(),earlier.entries.end(),later.entries.begin()));
}
void quota(const std::filesystem::path& directory,const Package& p,std::size_t added) {
  const auto used=io::bytes_used(directory);
  if(used>p.config.byte_quota || added>p.config.byte_quota-used)throw Failure("QUOTA_EXCEEDED");
}
}

struct Store::Impl {
  std::filesystem::path directory;
  std::unique_ptr<io::Lock> lock;
  std::unique_ptr<Package> package;
  std::unique_ptr<world::World> world;
  FaultHook hook;
  bool sealed=false;
  Impl(std::filesystem::path path,FaultHook fault):directory(std::move(path)),hook(std::move(fault)){}
  void phase(Phase phase){if(hook)hook(phase);}
  Result result(const char* code="") const {
    Result r;r.high_water=r.durable_revision=package->high();r.low_water=package->low();r.recovery_required=sealed;r.code=code;return r;
  }
  void checkpoint(const Bytes& bytes) {
    {
      io::File file(directory/"checkpoint.tmp",false);file.write(bytes);phase(Phase::before_checkpoint_flush);file.flush();
    }
    phase(Phase::after_checkpoint_flush);
    // Verify actual flushed bytes before selecting them. No fallback on failure.
    if(io::read(directory/"checkpoint.tmp",package_limit+frame_overhead)!=bytes)corrupt();
    phase(Phase::before_checkpoint_replace);io::replace_checkpoint(directory);phase(Phase::after_checkpoint_replace);
  }
  class Observer final:public transactions::CommitObserver {
   public:
    Observer(Impl& owner,std::string payload,std::uint64_t tick,Result& result):
      owner_(owner),payload_(std::move(payload)),tick_(tick),result_(result){}
    void prepare(const world::Snapshot& staged,const transactions::Receipt& receipt) override {
      prepared_=std::make_unique<Package>(*owner_.package);
      Entry entry{prepared_->high()+1,tick_,payload_,receipt.created,world::canonical_bytes(staged)};
      if(entry.checkpoint.size()>replay::checkpoint_limit)throw Failure("BUDGET_EXCEEDED");
      prepared_->entries.push_back(std::move(entry));
      auto durable=receipt;durable.durability="durable";prepared_->receipts.push_back(durable);
      if(prepared_->receipts.size()>receipt_limit)prepared_->receipts.erase(prepared_->receipts.begin());
      result_.receipt=std::move(durable);result_.durable_revision=result_.high_water=prepared_->high();result_.low_water=prepared_->low();
      const auto bytes=frame(*prepared_);quota(owner_.directory,*prepared_,bytes.size());
      // Every allocation for state, receipt and frame precedes durable I/O.
      touched_=true;io::File file(owner_.directory/"journal.bin",true);
      file.write(std::span<const std::uint8_t>(bytes).first(bytes.size()-40));
      owner_.phase(Phase::before_blob_flush);file.flush();
      file.write(std::span<const std::uint8_t>(bytes).last(40));
      owner_.phase(Phase::before_journal_flush);file.flush();
      owner_.phase(Phase::after_journal_flush);
    }
    void publish() noexcept override {owner_.package.swap(prepared_);}
    bool touched() const noexcept{return touched_;}
   private:
    Impl& owner_;std::string payload_;std::uint64_t tick_;Result& result_;
    std::unique_ptr<Package> prepared_;bool touched_=false;
  };
};
Store::Store(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
Store::~Store()=default;
std::unique_ptr<Store> Store::create(const std::filesystem::path& directory,const Config& config,FaultHook hook) {
  try {
    if(config.world_id.empty() || config.world_id.size()>128 || config.max_slots==0 || config.max_slots>8 || config.byte_quota>maximum_quota)
      throw Failure("INVALID_CONFIG");
    auto impl=std::make_unique<Impl>(directory,std::move(hook));impl->package=std::make_unique<Package>();impl->package->config=config;
    impl->world=std::make_unique<world::World>(config.world_id,config.seed,config.max_slots);impl->package->epoch=io::random_epoch();
    auto store=std::unique_ptr<Store>(new Store(std::move(impl)));auto& live=*store->impl_;
    if(!std::filesystem::create_directory(directory))throw Failure("STORE_EXISTS");
    io::check_directory(directory);live.lock=std::make_unique<io::Lock>(directory);
    const auto bytes=frame(*live.package);quota(directory,*live.package,bytes.size());
    {io::File journal(directory/"journal.bin",false);journal.flush();}
    live.checkpoint(bytes);io::sync_directory(directory);
    if(!directory.parent_path().empty())io::sync_directory(directory.parent_path());
    return store;
  } catch(const Failure&){throw;}
    catch(...){throw Failure("IO_ERROR");}
}
std::unique_ptr<Store> Store::open(const std::filesystem::path& directory,FaultHook hook) {
  try {
    io::check_directory(directory);auto impl=std::make_unique<Impl>(directory,std::move(hook));impl->lock=std::make_unique<io::Lock>(directory);
    const auto checkpoint=io::read(directory/"checkpoint.bin",package_limit+frame_overhead);
    std::size_t at=0;const auto selected=read_frame(checkpoint,at);check(selected && at==checkpoint.size());
    auto restored=decode(selected->payload);check(restored.package->high()==selected->high);
    auto selected_package=std::make_unique<Package>(*restored.package);
    const auto journal=io::read(directory/"journal.bin",journal_limit);at=0;std::unique_ptr<Package> prior;
    while(at<journal.size()) {
      const auto found=read_frame(journal,at);if(!found)break;
      auto candidate=decode(found->payload);check(found->high==candidate.package->high() && found->high>0);
      if(prior){check(candidate.package->high()==prior->high()+1);prefix(*prior,*candidate.package);}
      else check(candidate.package->high()<=selected_package->high()+1);
      if(candidate.package->high()<=selected_package->high())prefix(*candidate.package,*selected_package);
      else {check(candidate.package->high()==restored.package->high()+1);prefix(*restored.package,*candidate.package);}
      prior=std::make_unique<Package>(*candidate.package);
      if(candidate.package->high()>restored.package->high())restored=std::move(candidate);
    }
    impl->package=std::move(restored.package);impl->world=std::move(restored.world);
    if(io::bytes_used(directory)>impl->package->config.byte_quota)throw Failure("QUOTA_EXCEEDED");
    const bool scratch=std::filesystem::exists(directory/"checkpoint.tmp");
    if(scratch && std::filesystem::file_size(directory/"checkpoint.tmp")>package_limit+frame_overhead)corrupt();
    // Only now, after complete validation, repair the bounded uncommitted tail.
    if(at<journal.size()){io::File file(directory/"journal.bin",true);file.truncate(at);file.flush();}
    if(scratch) {
      std::filesystem::remove(directory/"checkpoint.tmp");io::sync_directory(directory);
    }
    return std::unique_ptr<Store>(new Store(std::move(impl)));
  } catch(const Failure&){throw;}
    catch(...){throw Failure("IO_ERROR");}
}
world::Snapshot Store::snapshot() const {
  if(impl_->sealed)throw Failure("RECOVERY_REQUIRED");return impl_->world->snapshot();
}
const std::string& Store::epoch() const noexcept{return impl_->package->epoch;}
std::uint64_t Store::durable_revision() const noexcept{return impl_->package->high();}
std::uint64_t Store::high_water() const noexcept{return impl_->package->high();}
std::uint64_t Store::low_water() const noexcept{return impl_->package->low();}
bool Store::recovery_required() const noexcept{return impl_->sealed;}
Result Store::lookup(std::uint64_t sequence) const {
  if(impl_->sealed)return impl_->result("RECOVERY_REQUIRED");
  if(sequence<low_water())return impl_->result("REQUIRES_RESYNC");
  if(sequence>high_water())return impl_->result("NOT_FOUND");
  auto result=impl_->result();result.receipt=impl_->package->receipts[static_cast<std::size_t>(sequence-low_water())];result.replayed=true;return result;
}
Result Store::apply_await_durable(const commands::Envelope& envelope,std::uint64_t tick) {
  if(impl_->sealed)return impl_->result("RECOVERY_REQUIRED");
  if(!bounded(envelope))return impl_->result("BUDGET_EXCEEDED");
  const auto serialized=commands::serialize(envelope);
  if(!serialized.json)return impl_->result("INVALID_SCHEMA");
  if(serialized.json->size()>replay::command_limit)return impl_->result("BUDGET_EXCEEDED");
  if(envelope.idempotency.epoch!=epoch())return impl_->result("EPOCH_MISMATCH");
  const auto sequence=envelope.idempotency.sequence;
  if(sequence<low_water())return impl_->result("REQUIRES_RESYNC");
  if(sequence<=high_water()) {
    if(*serialized.json!=impl_->package->entries[static_cast<std::size_t>(sequence-1)].payload)return impl_->result("IDEMPOTENCY_CONFLICT");
    auto result=lookup(sequence);
    try{impl_->phase(Phase::before_ack);}catch(...){impl_->sealed=true;return impl_->result("OUTCOME_UNKNOWN");}
    return result;
  }
  if(sequence!=high_water()+1)return impl_->result("SEQUENCE_GAP");
  if(high_water()==commit_limit)return impl_->result("BUDGET_EXCEEDED");
  if(!impl_->package->entries.empty() && tick<impl_->package->entries.back().tick)return impl_->result("INVALID_TICK");
  auto result=impl_->result();Impl::Observer observer(*impl_,*serialized.json,tick,result);
  try {
    auto receipt=transactions::Coordinator::apply_at_boundary(*impl_->world,envelope,nullptr,&observer);
    if(receipt.status!="committed") {
      result=impl_->result(receipt.errors.empty()?"INVALID_SCHEMA":receipt.errors.front().code.c_str());result.receipt=std::move(receipt);return result;
    }
    impl_->phase(Phase::before_ack);return result;
  } catch(const Failure& error) {
    if(!observer.touched())return impl_->result(error.code());
    impl_->sealed=true;return impl_->result("OUTCOME_UNKNOWN");
  } catch(...) {
    if(!observer.touched())return impl_->result("RESOURCE_EXHAUSTED");
    impl_->sealed=true;return impl_->result("OUTCOME_UNKNOWN");
  }
}
Result Store::compact() {
  if(impl_->sealed)return impl_->result("RECOVERY_REQUIRED");
  auto result=impl_->result();bool touched=false;
  try {
    const auto bytes=frame(*impl_->package);quota(impl_->directory,*impl_->package,bytes.size());
    touched=true;impl_->checkpoint(bytes);
    impl_->phase(Phase::before_journal_reclaim);
    {io::File file(impl_->directory/"journal.bin",false);file.flush();}
    io::sync_directory(impl_->directory);impl_->phase(Phase::after_journal_reclaim);return result;
  } catch(const Failure& error) {
    if(!touched)return impl_->result(error.code());
    impl_->sealed=true;return impl_->result("OUTCOME_UNKNOWN");
  } catch(...) {
    if(!touched)return impl_->result("RESOURCE_EXHAUSTED");
    impl_->sealed=true;return impl_->result("OUTCOME_UNKNOWN");
  }
}
void Store::notify_acknowledged() {
  if(impl_->sealed)throw Failure("RECOVERY_REQUIRED");
  try{impl_->phase(Phase::after_ack);}catch(...){impl_->sealed=true;throw Failure("OUTCOME_UNKNOWN");}
}
} // namespace ow::persistence
