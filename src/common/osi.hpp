// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <exception>
#include <memory>
#include <cstdint>

class FName;
struct sockaddr_in;
namespace OSI {
  using uint = unsigned int;
  void handle_signal(void (*handler)(int)) noexcept;
  void prevent_multirun(const FName &fname) noexcept;
  char *strtok(char *str, const char *delim, char **saveptr) noexcept;
  void binary2text(char *msg, uint &len, char &ch_last) noexcept;
  bool has_parent() noexcept;
  uint get_pid() noexcept;
  uint get_ppid() noexcept;

  class DirLock {
    std::unique_ptr<class dirlock_impl> _impl;
  public:
    explicit DirLock(const char *dwght) noexcept;
    ~DirLock() noexcept;
  };

  class Semaphore {
    std::unique_ptr<class sem_impl> _impl;
  public:
    static void cleanup() noexcept;
    explicit Semaphore() noexcept;
    ~Semaphore() noexcept;
    void open(const char *name, bool flag_create, uint value) noexcept;
    void close() noexcept;
    void inc() noexcept;
    void dec_wait() noexcept;
    int dec_wait_timeout(uint timeout) noexcept;
    bool ok() const noexcept;
  };

  class MMap {
    std::unique_ptr<class mmap_impl> _impl;
  public:
    static void cleanup() noexcept;
    explicit MMap() noexcept;
    ~MMap() noexcept;
    void open(const char *name, bool flag_create, size_t size) noexcept;
    void close() noexcept;
    void *operator()() const noexcept;
    bool ok() const noexcept;
  };

  class ReadHandle {
    std::unique_ptr<class rh_impl> _impl;
  public:
    explicit ReadHandle() noexcept;
    explicit ReadHandle(const class rh_impl &_impl) noexcept;
    ReadHandle(ReadHandle &&rh) noexcept;
    ReadHandle &operator=(ReadHandle &&rh) noexcept;
    ~ReadHandle() noexcept;
    void clear() noexcept;
    bool ok() const noexcept;
    unsigned int operator()(char *buf, uint size) const noexcept;
  };

  class ChildProcess {
    std::unique_ptr<class cp_impl> _impl;
  public:
    explicit ChildProcess() noexcept;
    ~ChildProcess() noexcept;
    void open(const char *path, char * const argv[]) noexcept;
    void close() noexcept;
    ReadHandle gen_handle_in() const noexcept;
    ReadHandle gen_handle_err() const noexcept;
    uint get_pid() const noexcept;
    void close_write() const noexcept;
    bool is_closed() const noexcept;
    bool ok() const noexcept;
    size_t write(const char *msg, size_t n) const noexcept;
  };
  
  class Dir {
    std::unique_ptr<class Dir_impl> _impl;
  public:
    explicit Dir(const char *dname) noexcept;
    ~Dir() noexcept;
    const char *next() const noexcept; };

  class Conn {
    std::unique_ptr<class Conn_impl> _impl;
  public:
    explicit Conn(const char *saddr, uint port);
    ~Conn() noexcept;
    bool ok() const noexcept;
    void connect() const;
    void send(const char *buf, size_t len, uint timeout, uint bufsiz) const;
    void recv(char *buf, size_t len, uint timeout, uint bufsiz) const; };

  class IAddr {
    uint64_t _crc64;
    uint32_t _addr;
    uint16_t _port;
    char _cipv4[24];

  public:
    explicit IAddr() noexcept { _cipv4[0] = '\0'; }
    ~IAddr() noexcept {}
    IAddr & operator=(const IAddr &) = default;
    IAddr(const IAddr &) = delete;
  
    explicit IAddr(const sockaddr_in &c_addr) noexcept { set_iaddr(c_addr); }
    explicit IAddr(const char *p, uint port) noexcept;
    void set_iaddr(const IAddr &iaddr) noexcept { *this = iaddr; }
    void set_iaddr(const sockaddr_in &c_addr) noexcept;

    const char *get_cipv4() const noexcept { return _cipv4; }
    uint64_t get_crc64() const noexcept { return _crc64; }
    uint32_t get_addr() const noexcept { return _addr; }
    uint16_t get_port() const noexcept { return _port; } };
}
