/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2014 Facebook, Inc. (http://www.facebook.com)     |
   | Copyright (c) 1997-2010 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef incl_HPHP_EXT_GENERATOR_H_
#define incl_HPHP_EXT_GENERATOR_H_


#include "hphp/runtime/ext/extension.h"
#include "hphp/runtime/base/builtin-functions.h"
#include "hphp/runtime/vm/resumable.h"
#include "hphp/runtime/vm/jit/types.h"
#include "hphp/system/systemlib.h"

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////
// class BaseGenerator

class BaseGenerator : public
      ExtObjectDataFlags<ObjectData::HasClone> {
public:
  enum class State : uint8_t {
    Created = 0,  // generator was created but never iterated
    Started = 1,  // generator was iterated but not currently running
    Running = 2,  // generator is currently being iterated
    Done    = 3   // generator has finished its execution
  };

  static constexpr ptrdiff_t resumableOff() { return -sizeof(Resumable); }
  static constexpr ptrdiff_t arOff() {
    return resumableOff() + Resumable::arOff();
  }
  static constexpr ptrdiff_t resumeAddrOff() {
    return resumableOff() + Resumable::resumeAddrOff();
  }
  static constexpr ptrdiff_t resumeOffsetOff() {
    return resumableOff() + Resumable::resumeOffsetOff();
  }
  static constexpr ptrdiff_t stateOff() {
    return offsetof(BaseGenerator, m_state);
  }

  explicit BaseGenerator(Class* cls)
    : ExtObjectDataFlags(cls, HeaderKind::ResumableObj)
  {}

  Resumable* resumable() const {
    return reinterpret_cast<Resumable*>(
      const_cast<char*>(reinterpret_cast<const char*>(this) + resumableOff()));
  }

  ActRec* actRec() const {
    return resumable()->actRec();
  }

  State getState() const {
    return m_state;
  }

  void setState(State state) {
    m_state = state;
  }

  void startedCheck() {
    if (getState() == State::Created) {
      throw_exception(
        SystemLib::AllocExceptionObject("Need to call next() first")
      );
    }
  }

  void preNext(bool checkStarted) {
    if (checkStarted) {
      startedCheck();
    }
    if (getState() == State::Running) {
      throw_exception(
        SystemLib::AllocExceptionObject("Generator is already running")
      );
    }
    if (getState() == State::Done) {
      throw_exception(
        SystemLib::AllocExceptionObject("Generator is already finished")
      );
    }
    assert(getState() == State::Created || getState() == State::Started);
    setState(State::Running);
  }

  /**
   * Get adjusted generator function base() where the real user code starts.
   *
   * Skips CreateCont and PopC opcodes.
   */
  static Offset userBase(const Func* func) {
    assert(func->isGenerator());
    auto base = func->base();

    DEBUG_ONLY auto op = reinterpret_cast<const Op*>(func->unit()->at(base));
    assert(op[0] == OpCreateCont);
    assert(op[1] == OpPopC);

    return base + 2;
  }

private:
  State m_state;
};


///////////////////////////////////////////////////////////////////////////////
// class Generator

class c_Generator : public BaseGenerator {
public:
  DECLARE_CLASS_NO_SWEEP(Generator)
  ~c_Generator();

  void t___construct();
  Variant t_current();
  Variant t_key();
  void t_next();
  void t_rewind();
  bool t_valid();
  void t_send(const Variant& v);
  void t_raise(const Variant& v);
  String t_getorigfuncname();
  String t_getcalledclass();

  static c_Generator* Clone(ObjectData* obj);

  template <bool clone>
  static c_Generator* Create(const ActRec* fp, size_t numSlots,
                             jit::TCA resumeAddr, Offset resumeOffset) {
    assert(fp);
    assert(fp->resumed() == clone);
    assert(fp->func()->isNonAsyncGenerator());
    void* obj = Resumable::Create<clone, sizeof(c_Generator)>(fp,
                                                              numSlots,
                                                              resumeAddr,
                                                              resumeOffset);
    auto const gen = new (obj) c_Generator();
    assert(gen->hasExactlyOneRef());
    assert(gen->noDestruct());
    gen->setState(State::Created);
    return gen;
  }

  void yield(Offset resumeOffset, const Cell* key, Cell value);
  void ret() { done(); }
  void fail() { done(); }

private:
  explicit c_Generator(Class* cls = c_Generator::classof());
  void copyVars(ActRec *fp);
  void done();

public:
  int64_t m_index;
  Cell m_key;
  Cell m_value;
};

///////////////////////////////////////////////////////////////////////////////
}

#endif // incl_HPHP_EXT_GENERATOR_H_
