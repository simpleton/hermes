/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the LICENSE
 * file in the root directory of this source tree.
 */
//===----------------------------------------------------------------------===//
/// \file
/// This file defines the standard layout of the HermesVM stack frame. It must
/// be shared between the compiler and the VM.
//===----------------------------------------------------------------------===//
#ifndef HERMES_BCGEN_HBC_STACKFRAMELAYOUT_H
#define HERMES_BCGEN_HBC_STACKFRAMELAYOUT_H

#include <cstdint>

namespace hermes {
namespace hbc {

/// Layout of a function stack frame from the point of view of the callee. Every
/// row is a HermesValue. Stack grows from high addresses (bottom of the table)
/// to low addresses (top of the table).
///
/// \verbatim
///   -----------------------------------------------
///   -3-N  callee localN    : HermesValue               -- stackPtr
///    ...
///   -3    callee local0    : HermesValue
///   -2    scratch          : HermesValue
///   -1    debugEnvironment : Environment*
///    ----------------------------------------------
///    0    previousFrame    : NativeValue(HermesValue*) -- calleeFramePtr
///    1    savedIP          : NativeValue(void*)
///    2    savedCodeBlock   : NativeValue(CodeBlock*)
///    3    argCount         : NativeValue(uint32_t)
///    4    newTarget        : Callable* | undefined
///    5    calleeClosureOrCB: Callable* | NativeValue(CodeBlock*)
///    6    this             : HermesValue
///    7    arg0             : HermesValue
///    ...
///    7+N  argN             : HermesValue
///    ...
///    ...
///         caller local 0   : HermesValue
///         scratch          : HermesValue
///         debugEnvironment : Environment*
///    ----------------------------------------------
///                                                      -- callerFramePtr
/// \endverbatim
///
/// The registers in the range [stackPtr .. calleeFramePtr-1] belong to the
/// callee frame. In other words, in the table above the negative offsets are
/// in the callee frame and the non-negative ones are in the caller frame.
///
/// Each function is responsible for allocating enough space in its own frame by
/// manipulating the stack pointer. So, to make a call, the caller must have
/// ensured enough registers at the top of its frame for the for the metadata
/// [previousFrame..calleeClosureOrCB] and the arguments [this, arg0..argN].
///
/// This is the sequence of events when performing a call:
/// - The caller allocates enough space for [previousFrame..argN] by subtracting
/// from the stack pointer. (That doesn't need to happen immediately before the
/// call.)
/// - The caller populates "argN..arg0" and "this".
/// - The caller populates calleeClosureOrCB, newTarget and argCount.
/// - The caller saves the current CodeBlock, IP and frame offset in the
/// corresponding fields.
/// - "debugEnvironment" is initialized to "undefined". (It will be populated
/// later by the callee.)
/// - Execution is transferred to callee.
/// - The callee updates the global "frame" register to point to the top of the
/// stack, i.e. the row labelled "0" in the table.
/// - The callee allocates registers in this frame by subtracting from the stack
/// pointer and continues execution.
///
/// When performing a return, the sequence is simpler:
/// - The code block and the IP are restored from the caller's frame.
/// - The callee moves the "frame" register to the "stackPointer" register.
/// - The callee moves the previous "frame" into the current frame register.
/// - Execution continues in the caller.
///
struct StackFrameLayout {
  enum {
    /// Offset of the first local register.
    FirstLocal = -3,
    /// A scratch register for use by the VM.
    Scratch = -2,
    /// The environment associated with the callee's stack frame, that is, the
    /// Environment created by the last CreateEnvironment instruction to execute
    /// in the callee's stack frame. It is null if debugging support is not
    /// present, or if no CreateEnvironment instruction has executed, which is
    /// possible if we are early in the code block, or with optimized code. This
    /// is stored in the call frame so that the debugger can gain access to the
    /// Environment at arbitrary frames. Note this is managed by the GC.
    DebugEnvironment = -1,
    /// Saved value of the caller's "frame" register, which points to the first
    /// register of the caller's stack frame.
    PreviousFrame = 0,
    /// Saved caller instruction pointer.
    SavedIP = 1,
    /// Saved caller CodeBlock.
    /// NOTE: If SavedCodeBlock is null but SavedIP is non-null, the current
    /// frame is the result of a bound function call - the SavedCodeBlock can be
    /// found using CalleeClosureOrCB on the previous call frame, but the
    /// SavedIP should have been saved by the bound call in the current frame.
    SavedCodeBlock = 2,
    /// Number of JavaScript arguments passed to the callee excluding "this".
    ArgCount = 3,
    /// The value of `new.target`. If constructing, it contains the callable of
    /// the constructor invoked by `new`, otherwise `undefined`.
    NewTarget = 4,
    /// The JavaScript Function object representing the callee, or a CodeBlock *
    /// representing the callee when CallDirect is used. The latter is ONLY
    /// valid if it is known at compile time that the callee doesn't need to
    /// access its closure (i.e. no non-strict Arguments.callee, etc).
    CalleeClosureOrCB = 5,
    /// The "this" argument.
    ThisArg = 6,
    /// The first explicit argument.
    FirstArg = 7,

    /// The number of registers the caller needs to allocate at the end of its
    /// frame in addition to its locals and the explicit argument registers. In
    /// other words, this includes all registers starting from \c
    /// CalleeClosureorCB up to the top of the frame.
    CallerExtraRegistersAtEnd = CalleeClosureOrCB - PreviousFrame + 1,

    /// The number of additional registers the callee needs to allocate in the
    /// beginning of its frame.
    CalleeExtraRegistersAtStart = DebugEnvironment - Scratch + 1,

    /// Direction of the stack.
    StackIncrement = -1,
  };

  /// Calculate the number of register slots needed for an outgoing call: it
  /// contains the outgoing arguments and the metadata. This saturates: on
  /// overflow it returns UINT32_MAX. Note that an overflow return is
  /// conceptually "too small" in that the true number of registers required
  /// would exceed the capacity of a uint32. The assumption is that the Runtime
  /// bounds the register stack max size below UINT32_MAX, and therefore will
  /// fail to allocate this "too small" size.
  /// \param numArgsExcludingThis number of arguments excluding \c thisArg
  /// \return the number of registers needed, or UINT32_MAX if the count
  /// would overflow.
  static uint32_t callerOutgoingRegisters(uint32_t numArgsExcludingThis) {
    // The >= pattern is specially recognized as an overflow check.
    uint32_t totalCount = FirstArg + numArgsExcludingThis;
    return totalCount >= numArgsExcludingThis ? totalCount : UINT32_MAX;
  }

  /// \return the offset of the register containing the N-th argument to the
  /// callee. -1 is this, 0 is the first explicit argument.
  static int32_t argOffset(int32_t n) {
    return FirstArg + n;
  }

  /// \return the offset of the local register with index n.
  static int32_t localOffset(int32_t n) {
    return FirstLocal - n;
  }
};

} // namespace hbc
} // namespace hermes

#endif // HERMES_BCGEN_HBC_STACKFRAMELAYOUT_H
