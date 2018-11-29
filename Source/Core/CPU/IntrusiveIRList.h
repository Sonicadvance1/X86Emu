#pragma once
#include <cstddef>
#include <cstring>
#include <tuple>
#include <vector>
#include "IR.h"

namespace Emu::IR {

class IntrusiveIRList {
public:
  IntrusiveIRList(size_t InitialSize) {
    IRList.resize(InitialSize);
  }

  IntrusiveIRList(const IntrusiveIRList &Other) {
    if (Other.CurrentOffset == 0)
      return;
    CurrentOffset = Other.CurrentOffset;
    IRList.resize(Other.CurrentOffset);
    memcpy(&IRList.at(0), &Other.IRList.at(0), Other.CurrentOffset);
  }

  template <class T>
  void CheckSize() { if ((CurrentOffset + sizeof(T)) > IRList.size()) IRList.resize(IRList.size() * 2); }

  // XXX: Clean this up
  template<class T, IROps T2>
  std::pair<T*, AlignmentType> AllocateOp() {
    size_t OpEnum = IR::IRSizes[T2];
    CheckSize<T>();
    auto Op = reinterpret_cast<T*>(&IRList.at(CurrentOffset));
    Op->Header.Op = T2;
    AlignmentType Offset = CurrentOffset;
    CurrentOffset += Emu::IR::GetSize(T2);
    return std::make_pair(Op, Offset);
  }

  IntrusiveIRList&
  operator=(IntrusiveIRList Other) {
    IRList.resize(Other.CurrentOffset);
    memcpy(&IRList.at(0), &Other.IRList.at(0), Other.CurrentOffset);
    return *this;
  }

  size_t GetOffset() const { return CurrentOffset; }

  void Reset() { CurrentOffset = 0; }

  IROp_Header const* GetOp(size_t Offset) const {
    return reinterpret_cast<IROp_Header const*>(&IRList.at(Offset));
  }

  template<class T>
  T const* GetOpAs(size_t Offset) const {
    return reinterpret_cast<T const*>(&IRList.at(Offset));
  }

private:
  AlignmentType CurrentOffset{0};
  std::vector<uint8_t> IRList;
};
}
