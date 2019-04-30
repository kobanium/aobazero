// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include "err.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <cassert>
#include <cstdint>
#include <climits>

namespace HTAux { uint16_t pearson16(const uint8_t *x, uint len) noexcept; }

class Key64 {
  using uint = unsigned int;
  uint64_t _u64;
  
public:
  explicit Key64() noexcept {}
  explicit Key64(uint64_t key) noexcept : _u64(key) {}
  Key64 & operator=(const Key64 & key) noexcept {
    _u64 = key._u64; return *this; }
  explicit operator uint() const noexcept { return static_cast<uint>(_u64); }
  bool operator==(const Key64 &key) const noexcept { return _u64 == key._u64; }
};

template<typename Key, typename Value>
class HashTable {
  using uint  = unsigned int;
  using uchar = unsigned char;
  struct Entry {
    Entry *f, **bfp;
    Entry *used_f, **used_bfp;
    Key key; Value value; };
  std::unique_ptr<Entry []> _ptr_entry;
  std::unique_ptr<Entry *[]> _ptr_f;
  Entry *_unused_f, *_used_f;
  Entry **_used_bfp;
  uint _nentry, _nused, _nindex, _mask;
  
public:
  explicit HashTable() noexcept {}
  explicit HashTable(uint log2_index, uint nentry) noexcept {
    reset(log2_index, nentry); }
  void reset(uint log2_index, uint nentry) noexcept {
    assert(0U < log2_index && 1U < nentry);
    _nindex = (1U << log2_index);
    _mask   = _nindex - 1U;
    _nused  = 0;
    _nentry = nentry;
    _ptr_entry.reset(new Entry [_nentry]);
    _ptr_f.reset(new Entry *[_nindex]);
    std::fill_n(_ptr_f.get(), _nindex, nullptr);
    
    _used_f   = nullptr;
    _used_bfp = &( _used_f );
    _unused_f = &( _ptr_entry[0] );
    for (uint u = 0; u < _nentry - 1U; ++u)
      _ptr_entry[u].used_f = &( _ptr_entry[u + 1U] );
    _ptr_entry[_nentry - 1U].used_f = nullptr; }
  
  Value & operator[](const Key &key) noexcept {
    uint index = static_cast<uint>(key) & _mask;
    Entry **bfp = &( _ptr_f[index] );
    for (; (*bfp) != nullptr; bfp = &( (*bfp)->f ))
      if ((*bfp)->key == key) {
	Entry *target = (*bfp);
	
	if (target->used_f) {
	  target->used_f->used_bfp = target->used_bfp;
	  *(target->used_bfp) = target->used_f;

	  target->used_f = nullptr;
	  target->used_bfp = _used_bfp;
	  
	  *_used_bfp = target;
	  _used_bfp = &( target->used_f ); }

	return target->value; }

    Entry *target;
    if (_unused_f == nullptr) {
      target            = _used_f;
      _used_f           = _used_f->used_f;
      _used_f->used_bfp = &( _used_f );
      *(target->bfp)    = target->f;
      if (bfp == &( target->f )) bfp = target->bfp;
      if (target->f) target->f->bfp = target->bfp; }
    
    else {
      target    = _unused_f;
      _unused_f = _unused_f->used_f;
      _nused   += 1U; }

    target->key      = key;
    target->value    = Value();
    target->f        = nullptr;
    target->bfp      = bfp;
    (*bfp)           = target;
    target->used_f   = nullptr;
    target->used_bfp = _used_bfp;
    (*_used_bfp)     = target;
    _used_bfp        = &( target->used_f );
    
    return target->value; }

  uint get_nused() const noexcept { return _nused; }
  uint get_nentry() const noexcept { return _nentry; }
  bool ok() const noexcept {
    if (_nentry < _nused) return false;
    if (_mask + 1U != _nindex) return false;
    if (!_used_bfp) return false;
    
    std::unique_ptr<uchar []> flags(new uchar [_nentry]);
    std::fill_n(flags.get(), _nentry, 0);
    
    uint nentry = 0;
    Entry * const *used_bfp = &( _used_f );
    for (; *used_bfp != nullptr; used_bfp = &( (*used_bfp)->used_f )) {
      nentry += 1U;
      if (*used_bfp < _ptr_entry.get()) return false;
      if (_ptr_entry.get() + _nentry <= *used_bfp) return false;
      if ((*used_bfp)->used_bfp != used_bfp) return false;
      
      uint u = *used_bfp - _ptr_entry.get();
      if (flags[u]) return false;
      flags[u] = 1U; }
    
    if (used_bfp != _used_bfp) return false;
    if (nentry != _nused) return false;
    
    for (uint index = 0; index < _nindex; ++index) {
      Entry **bfp = &( _ptr_f[index] );
      for (; *bfp != nullptr; bfp = &( (*bfp)->f )) {
	if (*bfp < _ptr_entry.get()) return false;
	if (_ptr_entry.get() + _nentry <= *used_bfp) return false;
	if ((*bfp)->bfp != bfp) return false;
	
	uint u = *bfp - _ptr_entry.get();
	if (index != (static_cast<uint>((*bfp)->key) & _mask)) return false;
	if (flags[u] != 1U) return false;
	flags[u] = 2U; } }
    
    for (Entry *f = _unused_f; f != nullptr; f = f->used_f) {
      nentry += 1U;
      if (f < _ptr_entry.get()) return false;
      if (_ptr_entry.get() + _nentry <= f) return false;
      
      uint u = f - _ptr_entry.get();
      if (flags[u] != 0) return false;
      flags[u] = 3U; }
  
    if (nentry != _nentry) return false;
    for (uint u = 0; u < _nentry; ++u)
      if (flags[u] != 2U && flags[u] != 3U) return false;
    
    return true; }

  std::string dump() const noexcept {
    std::string str("unused: ");
    for (const Entry *f = _unused_f; f != nullptr; f = f->used_f) {
      uint u = f - _ptr_entry.get();
      str += std::to_string(u);
      str += " "; }
    str += "null\nused: ";
    
    for (const Entry *f = _used_f; f != nullptr; f = f->used_f) {
      uint u = f - _ptr_entry.get();
      str += std::to_string(u);
      str += " "; }
    str += "null\n";
    
    for (uint index = 0; index < _nindex; ++index) {
      str += std::to_string(index);
      str += ": ";
      for (const Entry *f = _ptr_f[index]; f != nullptr; f = f->f) {
	uint u = f - _ptr_entry.get();
	str += std::to_string(u);
	str += " "; }
      str += "null\n"; }
    return str; }
  
  Value & at(const Key &key) const noexcept {
    uint index = static_cast<uint>(key) & _mask;
    for (Entry *f = _ptr_f[index]; f != nullptr; f = f->f)
      if (f->key == key) return f->value;
    ErrAux::die(ERR_INT("out of range"));
    return _ptr_entry[0].value; /* never reaches */ }
};
