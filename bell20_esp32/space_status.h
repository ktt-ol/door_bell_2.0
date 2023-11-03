#pragma once

enum class SpaceStatus {
  Unknown,
  Closed,
  Keyholder,
  Member,
  Open,
  OpenPlus,
};

SpaceStatus from_string(const char* const status, const unsigned int length);
