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
  
  class Pipe {
    std::unique_ptr<class Pipe_impl> _impl;
    friend class Selector_impl;
  public:
    explicit Pipe() noexcept;
    ~Pipe() noexcept;
    void open(const char *, char * const []) noexcept;
    void close() noexcept;
    uint get_pid() const noexcept;
    void close_write() const noexcept;
    bool is_closed() const noexcept;
    bool ok() const noexcept;
    size_t write(const char *msg, size_t n) const noexcept;
    char *getline_in_block() const noexcept;
    char *getline_err_block() const noexcept;
    char *getline_in() const noexcept;
    char *getline_err() const noexcept; };

  class Selector {
    std::unique_ptr<class Selector_impl> _impl;
  public:
    explicit Selector() noexcept;
    ~Selector() noexcept;
    void reset() const noexcept;
    void add(const Pipe &pipe) const noexcept;
    void wait(uint sec, uint msec) const noexcept;
    bool try_getline_in(const Pipe &pipe, char **pmsg) const noexcept;
    bool try_getline_err(const Pipe &pipe, char **pmsg) const noexcept;
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
