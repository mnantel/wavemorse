#pragma once
#include <Arduino.h>

// Iambic keyer state machine (Curtis mode A/B) with debounced paddle inputs.
//
// poll() must be called continuously from loop(). Element starts/ends are
// reported through the onKeyDown/onKeyUp callbacks; debounced raw paddle
// states are exposed for MIDI passthrough.
class Keyer {
public:
  void (*onKeyDown)(char element) = nullptr; // '.' or '-'
  void (*onKeyUp)() = nullptr;

  void begin(uint8_t ditPin, uint8_t dahPin, uint16_t debounceMs) {
    _ditPin = ditPin;
    _dahPin = dahPin;
    _debounceMs = debounceMs;
    pinMode(_ditPin, INPUT_PULLUP);
    pinMode(_dahPin, INPUT_PULLUP);
  }

  void setWpm(uint8_t wpm) { _ditMs = 1200 / wpm; }
  void setIambicB(bool b) { _modeB = b; }

  bool ditPressed() const { return _dit; }
  bool dahPressed() const { return _dah; }
  bool keyed() const { return _state == SENDING; }

  void poll(uint32_t now) {
    bool wasDit = _dit, wasDah = _dah;
    debounce(now);
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

  void debounce(uint32_t now) {
    bool rawDit = digitalRead(_ditPin) == LOW;
    bool rawDah = digitalRead(_dahPin) == LOW;
    if (rawDit != _dit && now - _tDit >= _debounceMs) {
      _dit = rawDit;
      _tDit = now;
    }
    if (rawDah != _dah && now - _tDah >= _debounceMs) {
      _dah = rawDah;
      _tDah = now;
    }
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
  uint32_t _tDit = 0, _tDah = 0;
};
