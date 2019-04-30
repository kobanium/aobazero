// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <memory>

template <typename K, typename V> class HashTable;
class Listen {
  using uint = unsigned int;
  using time_point_t = std::chrono::time_point<std::chrono::system_clock>;
  std::atomic<bool> _bEndWorker;
  std::thread _thread;
  class Logger *_logger;
  std::unique_ptr<HashTable<class IAddrKey, class IAddrValue>> _maxconn_table;
  std::unique_ptr<struct sockaddr_in> _s_addr;
  std::unique_ptr<class AddrList> _deny_list, _ignore_list;
  std::unique_ptr<class Peer []> _pPeer;
  time_point_t _last_deny, _now;
  int _sckt_lstn;
  uint _max_accept, _max_recv, _playerTO, _selectTO_sec, _selectTO_usec;
  uint _max_send, _len_block, _maxconn_sec, _maxconn_min, _maxconn_len;
  uint _cutconn_min, _maxlen_com;

  explicit Listen() noexcept;
  ~Listen() noexcept;
  Listen(const Listen &) = delete;
  Listen & operator=(const Listen &) = delete;
  
  ssize_t send_wrap(const Peer &peer, const void *buf, size_t len,
		    int flags) noexcept;
  ssize_t recv_wrap(const Peer &peer, void *buf, size_t len,
		    int flags) noexcept;
  void handle_connect() noexcept;
  void handle_send(Peer &peer) noexcept;
  void handle_recv(Peer &peer) noexcept;
  void worker() noexcept;
  
public:
  static Listen & get() noexcept;
  void start(Logger *logger, uint port_player, uint backlog, uint selectTO,
	     uint playerTO, uint max_accept, uint max_recv, uint max_send,
	     uint len_block, uint maxconn_sec, uint maxconn_min,
	     uint cutconn_min, uint maxlen_com) noexcept;
  void wait() noexcept;
  void end() noexcept;
};
