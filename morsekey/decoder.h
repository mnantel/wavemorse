#pragma once
#include <Arduino.h>

// Turns the keyer's element stream into text. Character boundaries are
// detected after 2.5 dit-times of silence, word boundaries after 6.
class Decoder {
public:
  void (*onChar)(char c) = nullptr;                // decoded char (or ' ')
  void (*onPatternChange)(const char *pat) = nullptr; // in-progress ". - ." pattern

  void setWpm(uint8_t wpm) { _ditMs = 1200 / wpm; }

  void onElement(char e, uint32_t now) {
    if (_len < MAX_LEN)
      _pat[_len++] = e;
    _pat[_len] = 0;
    _lastElement = now;
    _pending = true;
    _spaceSent = false;
    if (onPatternChange)
      onPatternChange(_pat);
  }

  // Called from loop(); `keying` suppresses gap detection while an element
  // is still being sent.
  void poll(uint32_t now, bool keying) {
    if (keying) {
      _lastElement = now;
      return;
    }
    if (_pending && now - _lastElement > (_ditMs * 5) / 2) {
      char c = lookup(_pat);
      _len = 0;
      _pat[0] = 0;
      _pending = false;
      if (onChar)
        onChar(c);
      if (onPatternChange)
        onPatternChange(_pat);
    }
    if (!_pending && !_spaceSent && _lastElement != 0 &&
        now - _lastElement > _ditMs * 6) {
      _spaceSent = true;
      if (onChar)
        onChar(' ');
    }
  }

  const char *pattern() const { return _pat; }

private:
  static const uint8_t MAX_LEN = 8;

  char lookup(const char *pat) {
    struct Entry {
      const char *pat;
      char ch;
    };
    static const Entry TABLE[] = {
        {".-", 'A'},     {"-...", 'B'},   {"-.-.", 'C'},   {"-..", 'D'},
        {".", 'E'},      {"..-.", 'F'},   {"--.", 'G'},    {"....", 'H'},
        {"..", 'I'},     {".---", 'J'},   {"-.-", 'K'},    {".-..", 'L'},
        {"--", 'M'},     {"-.", 'N'},     {"---", 'O'},    {".--.", 'P'},
        {"--.-", 'Q'},   {".-.", 'R'},    {"...", 'S'},    {"-", 'T'},
        {"..-", 'U'},    {"...-", 'V'},   {".--", 'W'},    {"-..-", 'X'},
        {"-.--", 'Y'},   {"--..", 'Z'},   {"-----", '0'},  {".----", '1'},
        {"..---", '2'},  {"...--", '3'},  {"....-", '4'},  {".....", '5'},
        {"-....", '6'},  {"--...", '7'},  {"---..", '8'},  {"----.", '9'},
        {".-.-.-", '.'}, {"--..--", ','}, {"..--..", '?'}, {"-..-.", '/'},
        {"-...-", '='},  {".-.-.", '+'},  {"-....-", '-'}, {".--.-.", '@'},
        {"---...", ':'}, {"-.--.", '('},  {"-.--.-", ')'}, {".-..-.", '"'},
        {".----.", '\''}, {"..--.-", '_'}, {"-.-.--", '!'}, {"...-.-", '*'},
    };
    for (auto &e : TABLE)
      if (strcmp(pat, e.pat) == 0)
        return e.ch;
    return '#'; // unrecognized pattern
  }

  char _pat[MAX_LEN + 1] = {0};
  uint8_t _len = 0;
  uint16_t _ditMs = 60;
  uint32_t _lastElement = 0;
  bool _pending = false;
  bool _spaceSent = true;
};
