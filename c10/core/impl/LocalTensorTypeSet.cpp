#include <c10/core/impl/LocalTensorTypeSet.h>

#include <iostream>

namespace c10 {
namespace impl {

C10_DEFINE_bool(disable_variable_dispatch, false, "This flag forcibly disables the Variable code paths from executing, which currently breaks profiling in the process.");

namespace {

/// In the CAFFE2_FB_LIMITED_MOBILE_CAPABILITY build setting,
/// thread_local is not supported.
#ifndef CAFFE2_FB_LIMITED_MOBILE_CAPABILITY

// NB: POD, zero initialized!
thread_local PODLocalDispatchKeySet raw_local_tensor_type_set;

#else // defined(CAFFE2_FB_LIMITED_MOBILE_CAPABILITY)

static PODLocalDispatchKeySet raw_local_tensor_type_set;

#endif

} // anonymous namespace

LocalDispatchKeySet tls_local_tensor_type_set() {
  // Hack until variable performance is fixed
  if (FLAGS_disable_variable_dispatch) {
    raw_local_tensor_type_set.set_excluded(
      raw_local_tensor_type_set.excluded().add(
        DispatchKey::VariableTensorId));
  }
  return raw_local_tensor_type_set;
}

// An RAII guard could snapshot and restore the entire state (entire DispatchKeySet) as
// opposed to only snapshotting and restoring the state of its assigned DispatchKey.
// I'm not sure which is better.  If only the RAII API is used, the two choices are
// not distinguishable.
//
// However, if the guard chooses to snapshot and restore the entire DispatchKeySet,
// the interaction with the non-RAII API changes.  Consider this sequence of events:
// - An RAII guard is declared for a particular DispatchKey, but snapshots the entire
//   current DispatchKeySet.
// - A call to the non-RAII API changes the state for a different DispatchKey.
// - The RAII guard goes out of scope, restoring the entire DispatchKeySet it snapshotted
//   (which restores the state for its own assigned DispatchKey and wipes out the state
//   for the other DispatchKey set by the non-RAII API).

// RAII API

IncludeDispatchKeyGuard::IncludeDispatchKeyGuard(DispatchKey x)
  : tls_(&raw_local_tensor_type_set)
  , id_(x)
  , prev_state_(tls_->included().has(x)) {
  if (!prev_state_) {
    tls_->set_included(tls_->included().add(x));
  }
}

IncludeDispatchKeyGuard::~IncludeDispatchKeyGuard() {
  if (!prev_state_) {
    tls_->set_included(tls_->included().remove(id_));
  }
}

ExcludeDispatchKeyGuard::ExcludeDispatchKeyGuard(DispatchKey x)
  : tls_(&raw_local_tensor_type_set)
  , id_(x)
  , prev_state_(tls_->excluded().has(x)) {
  if (!prev_state_) {
    tls_->set_excluded(tls_->excluded().add(x));
  }
}

ExcludeDispatchKeyGuard::~ExcludeDispatchKeyGuard() {
  if (!prev_state_) {
    tls_->set_excluded(tls_->excluded().remove(id_));
  }
}

// Non-RAII API
// Please prefer using the RAII API. See declarations in LocalDispatchKeySet.h for details.

bool tls_is_dispatch_key_excluded(DispatchKey x) {
  return raw_local_tensor_type_set.excluded().has(x);
}

void tls_set_dispatch_key_excluded(DispatchKey x, bool desired_state) {
  auto* tls = &raw_local_tensor_type_set;
  bool current_state = tls->excluded().has(x);
  if (desired_state != current_state) {
    if (desired_state) {
      tls->set_excluded(tls->excluded().add(x));
    } else {
      tls->set_excluded(tls->excluded().remove(x));
    }
  }
}

bool tls_is_dispatch_key_included(DispatchKey x) {
  return raw_local_tensor_type_set.included().has(x);

}

void tls_set_dispatch_key_included(DispatchKey x, bool desired_state) {
  auto* tls = &raw_local_tensor_type_set;
  bool current_state = tls->included().has(x);
  if (desired_state != current_state) {
    if (desired_state) {
      tls->set_included(tls->included().add(x));
    } else {
      tls->set_included(tls->included().remove(x));
    }
  }
}

}} // namespace c10::impl
