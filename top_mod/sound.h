#pragma once

#include <Arduino.h>

class SongParser {
  public:
    struct Note {
      uint16_t frequency;
      uint32_t duration;
    };
  public:
    SongParser(const char* song)
      : ptr(song)
      , default_duration(4)
      , default_octave(0)
      , wholenote(0)
    {
       while (*ptr && *ptr != ':')
         ++ptr;
       if (*ptr)
         ++ptr;

       unsigned bpm = 63;
       char flag = 0;
       bool eq = false;
       uint32_t value = 0;
       for (; *ptr; ++ptr) {
         const char c = *ptr;
         switch (c) {
           case 'd':
           case 'o':
           case 'b':
             flag = c;
             break;
           case '=':
             eq = true;
             value = 0;
             break;
           case ',':
           case ':':
             eq = false;
             switch (flag) {
               case 'd':
                 if (value > 0 && value < 65)
                   default_duration = value;
                 break;
               case 'o':
                 if (value >= 1 && value <= 7)
                   default_octave = value;
                 break;
               case 'b':
                 if (value > 20 && value < 600)
                   bpm = value;
                 break;
             }
             break;
           default:
             if (eq && isdigit(c)) {
               value = (value * 10) + (c - '0');
             }
             break;
         }

         if (c == ':')
           break;
       }
       if (*ptr)
         ++ptr;

       wholenote = 60000L * 4 / bpm;
    }

    std::optional<Note> next() {
      if (!*ptr)
        return std::nullopt;
        
      while (*ptr == ' ')
        ++ptr;

      unsigned num = 0;
      while (*ptr && isdigit(*ptr))
        num = (num * 10) + (*ptr++ - '0');
      uint32_t duration = wholenote / (num ? num : default_duration);

      if (!*ptr)
        return std::nullopt;

      unsigned note = 0;
      switch (*ptr++) {
        case 'c':
          note = 1;
          break;
        case 'd':
          note = 3;
          break;
        case 'e':
          note = 5;
          break;
        case 'f':
          note = 6;
          break;
        case 'g':
          note = 8;
          break;
        case 'a':
          note = 10;
          break;
        case 'b':
          note = 12;
          break;
        case 'p':
          note = 0;
          break;
      }
      
      if (*ptr == '#') {
        ++ptr;
        if (note)
          ++note;
      }

      if (*ptr == '.') {
        ++ptr;
        duration += duration / 2;
      }

      unsigned scale = default_octave;
      if (isdigit(*ptr)) {
        scale = *ptr++ - '0'; 
      }

      while (*ptr == ',')
        ++ptr;

      if (note)
        note += (scale - 1) * 12;
      unsigned frequency = note ? ((note < NOTES_SIZE) ? NOTES[note] : NOTES[0]) : 0;

      return Note { frequency, duration };

    }
  private:
    const char* ptr;
    unsigned default_duration;
    unsigned default_octave;
    unsigned wholenote;
    static const uint16_t NOTES[];
    static const size_t NOTES_SIZE;
};


class SongPlayer {
  public:
    SongPlayer(uint8_t pin)
      : sound_pin(pin)
      , parser("")
      , note_start(0)
      , note_duration(0)
    {}

    void start_play(const char* song) {
      parser = SongParser(song);
    }

    bool loop() {
      const unsigned long ts = millis();
      if (ts - note_start < note_duration)
        return true;

      const auto note = parser.next();
      if (! note)
        return false;
      
      if (note->frequency)
        tone(sound_pin, note->frequency, note->duration);
      note_start = millis();
      note_duration = note->duration;
      return true;
    }

  private:
    const uint8_t sound_pin;
    SongParser parser;
    unsigned long note_start;
    uint32_t note_duration;
};
