#include "space_status.h"

#include "str.h"

SpaceStatus from_string(const char* const status, const unsigned int length) {
    Str s(status, length);
    if (s == "open") {
      return SpaceStatus::Open;
    } else if (s == "open+") {
      return SpaceStatus::OpenPlus;
    } else if (s == "member") {
      return SpaceStatus::Member;
    } else if (s == "keyholder") {
      return SpaceStatus::Keyholder;
    } else if (s == "none") {
      return SpaceStatus::Closed;
    } else {
      return SpaceStatus::Unknown;
    }
}

