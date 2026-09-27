// SPDX-License-Identifier: Apache-2.0
#include "persistence_io.hpp"
#include "omniweft/persistence.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstdio>
#include <limits>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ow::persistence::io {
namespace {
[[noreturn]] void failed() {throw Failure("IO_ERROR");}
void safe(const std::filesystem::path& path,bool directory=false) {
  const auto status=std::filesystem::symlink_status(path);
  if(status.type()==std::filesystem::file_type::not_found) return;
  if(directory?!std::filesystem::is_directory(status):!std::filesystem::is_regular_file(status))
    throw Failure("CORRUPT_STORE");
#ifdef _WIN32
  const auto attributes=GetFileAttributesW(path.c_str());
  if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
    throw Failure("CORRUPT_STORE");
#endif
}
}
Digest sha256(std::span<const std::uint8_t> input) {
  constexpr std::array<std::uint32_t,64> k{
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  std::array<std::uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  if(input.size()>std::numeric_limits<std::uint64_t>::max()/8) throw Failure("BUDGET_EXCEEDED");
  Bytes data(input.begin(),input.end());data.push_back(0x80);
  while(data.size()%64!=56)data.push_back(0);
  const auto bits=static_cast<std::uint64_t>(input.size())*8;
  for(unsigned i=8;i>0;--i)data.push_back(static_cast<std::uint8_t>(bits>>((i-1)*8)));
  for(std::size_t at=0;at<data.size();at+=64) {
    std::array<std::uint32_t,64> w{};
    for(std::size_t i=0;i<16;++i)for(std::size_t j=0;j<4;++j)w[i]=(w[i]<<8)|data[at+i*4+j];
    for(std::size_t i=16;i<64;++i) {
      const auto a=w[i-15],b=w[i-2];
      w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+(std::rotr(b,17)^std::rotr(b,19)^(b>>10));
    }
    auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
    for(std::size_t i=0;i<64;++i) {
      const auto t1=v+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
      const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
      v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
  }
  Digest out{};
  for(std::size_t i=0;i<8;++i)for(unsigned j=0;j<4;++j)out[i*4+j]=static_cast<std::uint8_t>(h[i]>>(24-j*8));
  return out;
}
std::string random_epoch() {
  std::array<std::uint8_t,32> bytes{};
#ifdef _WIN32
  if(BCryptGenRandom(nullptr,bytes.data(),static_cast<ULONG>(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)failed();
#else
  std::size_t count=0;
  while(count<bytes.size()) {
    const auto n=getrandom(bytes.data()+count,bytes.size()-count,0);
    if(n<0 && errno==EINTR)continue;
    if(n<=0)failed();count+=static_cast<std::size_t>(n);
  }
#endif
  constexpr char digits[]="0123456789abcdef";
  std::string out;out.reserve(64);
  for(const auto byte:bytes){out.push_back(digits[byte>>4]);out.push_back(digits[byte&15]);}
  return out;
}
void check_directory(const std::filesystem::path& directory) {
  safe(directory,true);
  if(!std::filesystem::is_directory(directory))throw Failure("CORRUPT_STORE");
  for(const auto& entry:std::filesystem::directory_iterator(directory)) {
    const auto name=entry.path().filename();
    if(name!="checkpoint.bin" && name!="checkpoint.tmp" && name!="journal.bin" && name!="writer.lock")
      throw Failure("CORRUPT_STORE");
    safe(entry.path());
  }
}
std::uint64_t bytes_used(const std::filesystem::path& directory) {
  check_directory(directory);std::uint64_t size=0;
  for(const auto& entry:std::filesystem::directory_iterator(directory)) {
    const auto n=entry.file_size();
    if(n>maximum_quota || size>maximum_quota-n)throw Failure("QUOTA_EXCEEDED");
    size+=n;
  }
  return size;
}
#ifdef _WIN32
struct File::State {HANDLE handle=INVALID_HANDLE_VALUE;~State(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}};
struct Lock::State {HANDLE handle=INVALID_HANDLE_VALUE;~State(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}};
Lock::Lock(const std::filesystem::path& directory):state_(std::make_unique<State>()) {
  const auto path=directory/"writer.lock";safe(path);
  state_->handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(state_->handle==INVALID_HANDLE_VALUE)throw Failure(GetLastError()==ERROR_SHARING_VIOLATION?"STORE_LOCKED":"IO_ERROR");
}
File::File(const std::filesystem::path& path,bool append):state_(std::make_unique<State>()) {
  safe(path);state_->handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,
    append?OPEN_ALWAYS:CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(state_->handle==INVALID_HANDLE_VALUE)failed();
  LARGE_INTEGER zero{};if(append && !SetFilePointerEx(state_->handle,zero,nullptr,FILE_END))failed();
}
void File::write(std::span<const std::uint8_t> data) {
  while(!data.empty()) {
    DWORD count=0;const auto size=static_cast<DWORD>(std::min<std::size_t>(data.size(),0x7fffffff));
    if(!WriteFile(state_->handle,data.data(),size,&count,nullptr)||!count)failed();data=data.subspan(count);
  }
}
void File::flush(){if(!FlushFileBuffers(state_->handle))failed();}
void File::truncate(std::uint64_t size) {
  LARGE_INTEGER at{};at.QuadPart=static_cast<LONGLONG>(size);
  if(!SetFilePointerEx(state_->handle,at,nullptr,FILE_BEGIN)||!SetEndOfFile(state_->handle))failed();
}
Bytes read(const std::filesystem::path& path,std::size_t limit) {
  safe(path);HANDLE handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(handle==INVALID_HANDLE_VALUE)failed();
  struct Close{HANDLE h;~Close(){CloseHandle(h);}} close{handle};
  LARGE_INTEGER size{};
  if(!GetFileSizeEx(handle,&size)||size.QuadPart<0)failed();
  if(static_cast<std::uint64_t>(size.QuadPart)>limit)throw Failure("CORRUPT_STORE");
  Bytes out(static_cast<std::size_t>(size.QuadPart));std::size_t at=0;
  while(at<out.size()) {DWORD count=0;
    if(!ReadFile(handle,out.data()+at,static_cast<DWORD>(out.size()-at),&count,nullptr)||!count)failed();at+=count;
  }
  return out;
}
void sync_directory(const std::filesystem::path&) {
  // Windows file data uses FlushFileBuffers; replacement uses WRITE_THROUGH.
  // This profile promises process-crash recovery, not controller/power-loss durability.
}
void replace_checkpoint(const std::filesystem::path& directory) {
  if(!MoveFileExW((directory/"checkpoint.tmp").c_str(),(directory/"checkpoint.bin").c_str(),
                 MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))failed();
}
#else
struct File::State {int fd=-1;~State(){if(fd>=0)::close(fd);}};
struct Lock::State {int fd=-1;~State(){if(fd>=0)::close(fd);}};
Lock::Lock(const std::filesystem::path& directory):state_(std::make_unique<State>()) {
  const auto path=directory/"writer.lock";safe(path);
  state_->fd=::open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
  if(state_->fd<0)failed();
  if(flock(state_->fd,LOCK_EX|LOCK_NB)!=0)throw Failure(errno==EWOULDBLOCK?"STORE_LOCKED":"IO_ERROR");
}
File::File(const std::filesystem::path& path,bool append):state_(std::make_unique<State>()) {
  safe(path);state_->fd=::open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW|(append?O_APPEND:O_TRUNC),0600);
  if(state_->fd<0)failed();
}
void File::write(std::span<const std::uint8_t> data) {
  while(!data.empty()) {
    const auto count=::write(state_->fd,data.data(),data.size());
    if(count<0 && errno==EINTR)continue;
    if(count<=0)failed();data=data.subspan(static_cast<std::size_t>(count));
  }
}
void File::flush(){if(::fsync(state_->fd)!=0)failed();}
void File::truncate(std::uint64_t size){if(::ftruncate(state_->fd,static_cast<off_t>(size))!=0)failed();}
Bytes read(const std::filesystem::path& path,std::size_t limit) {
  safe(path);const int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
  if(fd<0)failed();struct Close{int fd;~Close(){::close(fd);}} close{fd};
  struct stat state{};if(::fstat(fd,&state)!=0 || !S_ISREG(state.st_mode)||state.st_size<0)failed();
  if(static_cast<std::uint64_t>(state.st_size)>limit)throw Failure("CORRUPT_STORE");
  Bytes out(static_cast<std::size_t>(state.st_size));std::size_t at=0;
  while(at<out.size()) {const auto count=::read(fd,out.data()+at,out.size()-at);
    if(count<0 && errno==EINTR)continue;
    if(count<=0)failed();at+=static_cast<std::size_t>(count);
  }
  return out;
}
void sync_directory(const std::filesystem::path& path) {
  const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)failed();
  const int status=::fsync(fd);::close(fd);if(status!=0)failed();
}
void replace_checkpoint(const std::filesystem::path& directory) {
  if(::rename((directory/"checkpoint.tmp").c_str(),(directory/"checkpoint.bin").c_str())!=0)failed();
  sync_directory(directory);
}
#endif
File::~File()=default;
Lock::~Lock()=default;
} // namespace ow::persistence::io
