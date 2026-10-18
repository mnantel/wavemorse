#pragma once
#include <Arduino.h>

// Paddle edge interrupt: stamps the time into the pin's edge timestamp.
// A free function, not a class member: IRAM_ATTR on an inline member
// trips "dangerous relocation" at link time.
static void IRAM_ATTR keyerOnEdge(void *arg) {
  *(volatile uint32_t *)arg = micros();
}

// Iambic keyer state machine (Curtis mode A/B) with debounced paddle inputs.
//
// poll() must be called continuously from loop(). Element starts/ends are
// reported through the onKeyDown/onKeyUp callbacks; debounced raw paddle
// states are exposed for MIDI passthrough.
class Keyer {
public:
  void (*onKeyDown)(char element) = nullptr; // '.' or '-'
  void (*onKeyUp)() = nullptr;

  // Safe to call again (e.g. paddle swap): re-binds the edge interrupts.
  void begin(uint8_t ditPin, uint8_t dahPin, uint16_t debounceMs) {
    if (_attached) {
      detachInterrupt(_ditPin);
      detachInterrupt(_dahPin);
    }
    _ditPin = ditPin;
    _dahPin = dahPin;
    _debounceMs = debounceMs;
    pinMode(_ditPin, INPUT_PULLUP);
    pinMode(_dahPin, INPUT_PULLUP);
    attachInterruptArg(_ditPin, keyerOnEdge, (void *)&_edgeDit, CHANGE);
    attachInterruptArg(_dahPin, keyerOnEdge, (void *)&_edgeDah, CHANGE);
    _attached = true;
  }

  void setWpm(uint8_t wpm) { _ditMs = 1200 / wpm; }
  void setIambicB(bool b) { _modeB = b; }

  bool ditPressed() const { return _dit; }
  bool dahPressed() const { return _dah; }
  bool keyed() const { return _state == SENDING; }

  // Straight-key mode: debounce the contacts without running the keyer, and
  // read a physical pin's debounced state (independent of paddle swap).
  void pollContacts() { debounce(); }
  bool contact(uint8_t pin) const { return pin == _ditPin ? _dit : _dah; }

  void poll(uint32_t now) {
    bool wasDit = _dit, wasDah = _dah;
    debounce();
    bool ditEdge = _dit && !wasDit;
    bool dahEdge = _dah && !wasDah;

    switch (_state) {
    case IDLE:
      _ditQ = _dahQ = false;
      if (_dit)
        startElement('.', now);
      else if (_dah)
        startElement('-', now);
      break;

    case SENDING:
      latchMemories(ditEdge, dahEdge);
      if (now - _tStart >= (_element == '.' ? _ditMs : 3 * _ditMs)) {
        if (onKeyUp)
          onKeyUp();
        _state = GAP;
        _tStart = now;
      }
      break;

    case GAP:
      latchMemories(ditEdge, dahEdge);
      if (now - _tStart >= _ditMs) {
        char next = decideNext();
        if (next)
          startElement(next, now);
        else
          _state = IDLE;
      }
      break;
    }
  }

private:
  enum State { IDLE, SENDING, GAP };

  void startElement(char e, uint32_t now) {
    _element = e;
    _state = SENDING;
    _tStart = now;
    if (e == '.')
      _ditMem = false;
    else
      _dahMem = false;
    if (onKeyDown)
      onKeyDown(e);
  }

  // While sending (and during the gap in mode B), a tap on the opposite
  // paddle is remembered. Mode A forgets those once both paddles are
  // released. Independently, a NEW press of the same paddle as the element
  // in flight queues exactly one repeat ("dot/dash memory") so quick
  // individual taps aren't swallowed by the element clock.
  void latchMemories(bool ditEdge, bool dahEdge) {
    if (_modeB || _state == SENDING) {
      if (_dit && _element == '-')
        _ditMem = true;
      if (_dah && _element == '.')
        _dahMem = true;
    }
    if (ditEdge && _element == '.')
      _ditQ = true;
    if (dahEdge && _element == '-')
      _dahQ = true;
    if (!_modeB && !_dit && !_dah)
      _ditMem = _dahMem = false;
  }

  char decideNext() {
    if (_element == '.') {
      if (_dahMem || _dah)
        return '-';
      if (_dit || _ditQ) {
        _ditQ = false;
        return '.';
      }
    } else {
      if (_ditMem || _dit)
        return '.';
      if (_dah || _dahQ) {
        _dahQ = false;
        return '-';
      }
    }
    return 0;
  }

  // Every contact edge is timestamped by interrupt, so bounces shorter than
  // a loop iteration still count. A new level is accepted only once its pin
  // has been edge-free for the debounce time: contact bounce (quiet gaps
  // <4ms measured on a ZN-SL) and crosstalk glitches induced by the other
  // contact switching never qualify; a real press or release always does.
  void debounce() {
    _dit = settled(_ditPin, _edgeDit, _dit);
    _dah = settled(_dahPin, _edgeDah, _dah);
  }

  // The contact's new level, or `cur` if it hasn't been edge-free for the
  // debounce time. The edge stamp is written by an interrupt, so it can be
  // newer than a micros() read taken just before it: compare signed, then
  // re-check after a moment so an edge whose interrupt is still landing
  // can't slip a mid-bounce level through.
  bool settled(uint8_t pin, volatile uint32_t &edge, bool cur) {
    bool raw = digitalRead(pin) == LOW;
    if (raw == cur)
      return cur;
    uint32_t e = edge;
    if ((int32_t)(micros() - e) < (int32_t)(_debounceMs * 1000u))
      return cur;
    delayMicroseconds(20);
    if (edge != e || (digitalRead(pin) == LOW) != raw)
      return cur;
    return raw;
  }

  uint8_t _ditPin = 0, _dahPin = 0;
  uint16_t _debounceMs = 5;
  uint16_t _ditMs = 60;
  bool _modeB = true;

  State _state = IDLE;
  char _element = '.';
  uint32_t _tStart = 0;
  bool _dit = false, _dah = false;
  bool _ditMem = false, _dahMem = false;
  bool _ditQ = false, _dahQ = false; // same-paddle tap queue
  volatile uint32_t _edgeDit = 0, _edgeDah = 0; // micros() of last edge
  bool _attached = false;
};
