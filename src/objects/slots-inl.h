// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_INL_H_
#define V8_OBJECTS_SLOTS_INL_H_

#include "src/base/atomic-utils.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/heap-object.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/objects/tagged.h"
#include "src/sandbox/external-pointer-inl.h"
#include "src/sandbox/indirect-pointer-inl.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

//
// FullObjectSlot implementation.
//

FullObjectSlot::FullObjectSlot(TaggedBase* object)
    : SlotBase(reinterpret_cast<Address>(&object->ptr_)) {}

bool FullObjectSlot::contains_map_value(Address raw_value) const {
  return load_map().ptr() == raw_value;
}

bool FullObjectSlot::Relaxed_ContainsMapValue(Address raw_value) const {
  return base::AsAtomicPointer::Relaxed_Load(location()) == raw_value;
}

Tagged<Object> FullObjectSlot::operator*() const {
  return Tagged<Object>(*location());
}

Tagged<Object> FullObjectSlot::load(PtrComprCageBase cage_base) const {
  return **this;
}

void FullObjectSlot::store(Tagged<Object> value) const {
  *location() = value.ptr();
}

void FullObjectSlot::store_map(Tagged<Map> map) const {
#ifdef V8_MAP_PACKING
  *location() = MapWord::Pack(map.ptr());
#else
  store(map);
#endif
}

Tagged<Map> FullObjectSlot::load_map() const {
#ifdef V8_MAP_PACKING
  return Map::unchecked_cast(Tagged<Object>(MapWord::Unpack(*location())));
#else
  return Map::unchecked_cast(Tagged<Object>(*location()));
#endif
}

Tagged<Object> FullObjectSlot::Acquire_Load() const {
  return Tagged<Object>(base::AsAtomicPointer::Acquire_Load(location()));
}

Tagged<Object> FullObjectSlot::Acquire_Load(PtrComprCageBase cage_base) const {
  return Acquire_Load();
}

Tagged<Object> FullObjectSlot::Relaxed_Load() const {
  return Tagged<Object>(base::AsAtomicPointer::Relaxed_Load(location()));
}

Tagged<Object> FullObjectSlot::Relaxed_Load(PtrComprCageBase cage_base) const {
  return Relaxed_Load();
}

void FullObjectSlot::Relaxed_Store(Tagged<Object> value) const {
  base::AsAtomicPointer::Relaxed_Store(location(), value.ptr());
}

void FullObjectSlot::Release_Store(Tagged<Object> value) const {
  base::AsAtomicPointer::Release_Store(location(), value.ptr());
}

Tagged<Object> FullObjectSlot::Relaxed_CompareAndSwap(
    Tagged<Object> old, Tagged<Object> target) const {
  Address result = base::AsAtomicPointer::Relaxed_CompareAndSwap(
      location(), old.ptr(), target.ptr());
  return Tagged<Object>(result);
}

Tagged<Object> FullObjectSlot::Release_CompareAndSwap(
    Tagged<Object> old, Tagged<Object> target) const {
  Address result = base::AsAtomicPointer::Release_CompareAndSwap(
      location(), old.ptr(), target.ptr());
  return Tagged<Object>(result);
}

//
// FullMaybeObjectSlot implementation.
//

MaybeObject FullMaybeObjectSlot::operator*() const {
  return MaybeObject(*location());
}

MaybeObject FullMaybeObjectSlot::load(PtrComprCageBase cage_base) const {
  return **this;
}

void FullMaybeObjectSlot::store(MaybeObject value) const {
  *location() = value.ptr();
}

MaybeObject FullMaybeObjectSlot::Relaxed_Load() const {
  return MaybeObject(base::AsAtomicPointer::Relaxed_Load(location()));
}

MaybeObject FullMaybeObjectSlot::Relaxed_Load(
    PtrComprCageBase cage_base) const {
  return Relaxed_Load();
}

void FullMaybeObjectSlot::Relaxed_Store(MaybeObject value) const {
  base::AsAtomicPointer::Relaxed_Store(location(), value->ptr());
}

void FullMaybeObjectSlot::Release_CompareAndSwap(MaybeObject old,
                                                 MaybeObject target) const {
  base::AsAtomicPointer::Release_CompareAndSwap(location(), old.ptr(),
                                                target.ptr());
}

//
// FullHeapObjectSlot implementation.
//

HeapObjectReference FullHeapObjectSlot::operator*() const {
  return HeapObjectReference(*location());
}

HeapObjectReference FullHeapObjectSlot::load(PtrComprCageBase cage_base) const {
  return **this;
}

void FullHeapObjectSlot::store(HeapObjectReference value) const {
  *location() = value.ptr();
}

Tagged<HeapObject> FullHeapObjectSlot::ToHeapObject() const {
  TData value = *location();
  DCHECK(HAS_STRONG_HEAP_OBJECT_TAG(value));
  return HeapObject::cast(Tagged<Object>(value));
}

void FullHeapObjectSlot::StoreHeapObject(Tagged<HeapObject> value) const {
  *location() = value.ptr();
}

void ExternalPointerSlot::init(Isolate* isolate, Address value) {
#ifdef V8_ENABLE_SANDBOX
  ExternalPointerTable& table = GetOwningTable(isolate);
  ExternalPointerHandle handle =
      table.AllocateAndInitializeEntry(GetOwningSpace(isolate), value, tag_);
  // Use a Release_Store to ensure that the store of the pointer into the
  // table is not reordered after the store of the handle. Otherwise, other
  // threads may access an uninitialized table entry and crash.
  Release_StoreHandle(handle);
#else
  store(isolate, value);
#endif  // V8_ENABLE_SANDBOX
}

#ifdef V8_ENABLE_SANDBOX
ExternalPointerHandle ExternalPointerSlot::Relaxed_LoadHandle() const {
  return base::AsAtomic32::Relaxed_Load(location());
}

void ExternalPointerSlot::Relaxed_StoreHandle(
    ExternalPointerHandle handle) const {
  return base::AsAtomic32::Relaxed_Store(location(), handle);
}

void ExternalPointerSlot::Release_StoreHandle(
    ExternalPointerHandle handle) const {
  return base::AsAtomic32::Release_Store(location(), handle);
}
#endif  // V8_ENABLE_SANDBOX

Address ExternalPointerSlot::load(const Isolate* isolate) {
#ifdef V8_ENABLE_SANDBOX
  const ExternalPointerTable& table = GetOwningTable(isolate);
  ExternalPointerHandle handle = Relaxed_LoadHandle();
  return table.Get(handle, tag_);
#else
  return ReadMaybeUnalignedValue<Address>(address());
#endif  // V8_ENABLE_SANDBOX
}

void ExternalPointerSlot::store(Isolate* isolate, Address value) {
#ifdef V8_ENABLE_SANDBOX
  ExternalPointerTable& table = GetOwningTable(isolate);
  ExternalPointerHandle handle = Relaxed_LoadHandle();
  table.Set(handle, value, tag_);
#else
  WriteMaybeUnalignedValue<Address>(address(), value);
#endif  // V8_ENABLE_SANDBOX
}

ExternalPointerSlot::RawContent
ExternalPointerSlot::GetAndClearContentForSerialization(
    const DisallowGarbageCollection& no_gc) {
#ifdef V8_ENABLE_SANDBOX
  ExternalPointerHandle content = Relaxed_LoadHandle();
  Relaxed_StoreHandle(kNullExternalPointerHandle);
#else
  Address content = ReadMaybeUnalignedValue<Address>(address());
  WriteMaybeUnalignedValue<Address>(address(), kNullAddress);
#endif
  return content;
}

void ExternalPointerSlot::RestoreContentAfterSerialization(
    ExternalPointerSlot::RawContent content,
    const DisallowGarbageCollection& no_gc) {
#ifdef V8_ENABLE_SANDBOX
  return Relaxed_StoreHandle(content);
#else
  return WriteMaybeUnalignedValue<Address>(address(), content);
#endif
}

void ExternalPointerSlot::ReplaceContentWithIndexForSerialization(
    const DisallowGarbageCollection& no_gc, uint32_t index) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(sizeof(ExternalPointerHandle) == sizeof(uint32_t));
  Relaxed_StoreHandle(index);
#else
  WriteMaybeUnalignedValue<Address>(address(), static_cast<Address>(index));
#endif
}

uint32_t ExternalPointerSlot::GetContentAsIndexAfterDeserialization(
    const DisallowGarbageCollection& no_gc) {
#ifdef V8_ENABLE_SANDBOX
  static_assert(sizeof(ExternalPointerHandle) == sizeof(uint32_t));
  return Relaxed_LoadHandle();
#else
  return static_cast<uint32_t>(ReadMaybeUnalignedValue<Address>(address()));
#endif
}

#ifdef V8_ENABLE_SANDBOX
const ExternalPointerTable& ExternalPointerSlot::GetOwningTable(
    const Isolate* isolate) {
  DCHECK_NE(tag_, kExternalPointerNullTag);
  return IsSharedExternalPointerType(tag_)
             ? isolate->shared_external_pointer_table()
             : isolate->external_pointer_table();
}

ExternalPointerTable& ExternalPointerSlot::GetOwningTable(Isolate* isolate) {
  DCHECK_NE(tag_, kExternalPointerNullTag);
  return IsSharedExternalPointerType(tag_)
             ? isolate->shared_external_pointer_table()
             : isolate->external_pointer_table();
}

ExternalPointerTable::Space* ExternalPointerSlot::GetOwningSpace(
    Isolate* isolate) {
  if (V8_UNLIKELY(IsSharedExternalPointerType(tag_))) {
    DCHECK(!ReadOnlyHeap::Contains(address()));
    return isolate->shared_external_pointer_space();
  }
  if (V8_UNLIKELY(ReadOnlyHeap::Contains(address()))) {
    DCHECK(tag_ == kAccessorInfoGetterTag || tag_ == kAccessorInfoSetterTag ||
           tag_ == kCallHandlerInfoCallbackTag);
    return isolate->heap()->read_only_external_pointer_space();
  }
  return isolate->heap()->external_pointer_space();
}
#endif  // V8_ENABLE_SANDBOX

Tagged<Object> IndirectPointerSlot::load(const Isolate* isolate) const {
  return Relaxed_Load(isolate);
}

void IndirectPointerSlot::store(Tagged<ExposedTrustedObject> value) const {
  return Relaxed_Store(value);
}

Tagged<Object> IndirectPointerSlot::Relaxed_Load(const Isolate* isolate) const {
  IndirectPointerHandle handle = Relaxed_LoadHandle();
  return ResolveHandle(handle, isolate);
}

Tagged<Object> IndirectPointerSlot::Acquire_Load(const Isolate* isolate) const {
  IndirectPointerHandle handle = Acquire_LoadHandle();
  return ResolveHandle(handle, isolate);
}

void IndirectPointerSlot::Relaxed_Store(
    Tagged<ExposedTrustedObject> value) const {
#ifdef V8_ENABLE_SANDBOX
  IndirectPointerHandle handle = value->ReadField<IndirectPointerHandle>(
      ExposedTrustedObject::kSelfIndirectPointerOffset);
  DCHECK_NE(handle, kNullIndirectPointerHandle);
  Relaxed_StoreHandle(handle);
#else
  UNREACHABLE();
#endif  // V8_ENABLE_SANDBOX
}

void IndirectPointerSlot::Release_Store(
    Tagged<ExposedTrustedObject> value) const {
#ifdef V8_ENABLE_SANDBOX
  IndirectPointerHandle handle = value->ReadField<IndirectPointerHandle>(
      ExposedTrustedObject::kSelfIndirectPointerOffset);
  Release_StoreHandle(handle);
#else
  UNREACHABLE();
#endif  // V8_ENABLE_SANDBOX
}

IndirectPointerHandle IndirectPointerSlot::Relaxed_LoadHandle() const {
  return base::AsAtomic32::Relaxed_Load(location());
}

IndirectPointerHandle IndirectPointerSlot::Acquire_LoadHandle() const {
  return base::AsAtomic32::Acquire_Load(location());
}

void IndirectPointerSlot::Relaxed_StoreHandle(
    IndirectPointerHandle handle) const {
  return base::AsAtomic32::Relaxed_Store(location(), handle);
}

void IndirectPointerSlot::Release_StoreHandle(
    IndirectPointerHandle handle) const {
  return base::AsAtomic32::Release_Store(location(), handle);
}

Tagged<Object> IndirectPointerSlot::ResolveHandle(
    IndirectPointerHandle handle, const Isolate* isolate) const {
#ifdef V8_ENABLE_SANDBOX
  // TODO(saelo) Maybe come up with a different entry encoding scheme that
  // returns Smi::zero for kNullCodePointerHandle?
  if (!handle) return Smi::zero();

  // Resolve the handle. The tag implies the pointer table to use.
  if (tag_ == kUnknownIndirectPointerTag) {
    // In this case we have to rely on the handle marking to determine which
    // pointer table to use.
    if (handle & kCodePointerHandleMarker) {
      return ResolveCodePointerHandle(handle);
    } else {
      return ResolveTrustedPointerHandle(handle, isolate);
    }
  } else if (tag_ == kCodeIndirectPointerTag) {
    return ResolveCodePointerHandle(handle);
  } else {
    return ResolveTrustedPointerHandle(handle, isolate);
  }
#else
  UNREACHABLE();
#endif  // V8_ENABLE_SANDBOX
}

#ifdef V8_ENABLE_SANDBOX
Tagged<Object> IndirectPointerSlot::ResolveTrustedPointerHandle(
    IndirectPointerHandle handle, const Isolate* isolate) const {
  DCHECK_NE(handle, kNullIndirectPointerHandle);
  const TrustedPointerTable& table = isolate->trusted_pointer_table();
  return Tagged<Object>(table.Get(handle));
}

Tagged<Object> IndirectPointerSlot::ResolveCodePointerHandle(
    IndirectPointerHandle handle) const {
  DCHECK_NE(handle, kNullIndirectPointerHandle);
  Address addr = GetProcessWideCodePointerTable()->GetCodeObject(handle);
  return Tagged<Object>(addr);
}
#endif  // V8_ENABLE_SANDBOX

//
// Utils.
//

// Copies tagged words from |src| to |dst|. The data spans must not overlap.
// |src| and |dst| must be kTaggedSize-aligned.
inline void CopyTagged(Address dst, const Address src, size_t num_tagged) {
  static const size_t kBlockCopyLimit = 16;
  CopyImpl<kBlockCopyLimit>(reinterpret_cast<Tagged_t*>(dst),
                            reinterpret_cast<const Tagged_t*>(src), num_tagged);
}

// Sets |counter| number of kTaggedSize-sized values starting at |start| slot.
inline void MemsetTagged(Tagged_t* start, Tagged<Object> value,
                         size_t counter) {
#ifdef V8_COMPRESS_POINTERS
  // CompressAny since many callers pass values which are not valid objects.
  Tagged_t raw_value = V8HeapCompressionScheme::CompressAny(value.ptr());
  MemsetUint32(start, raw_value, counter);
#else
  Address raw_value = value.ptr();
  MemsetPointer(start, raw_value, counter);
#endif
}

// Sets |counter| number of kTaggedSize-sized values starting at |start| slot.
template <typename T>
inline void MemsetTagged(SlotBase<T, Tagged_t> start, Tagged<Object> value,
                         size_t counter) {
  MemsetTagged(start.location(), value, counter);
}

// Sets |counter| number of kSystemPointerSize-sized values starting at |start|
// slot.
inline void MemsetPointer(FullObjectSlot start, Tagged<Object> value,
                          size_t counter) {
  MemsetPointer(start.location(), value.ptr(), counter);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_INL_H_
