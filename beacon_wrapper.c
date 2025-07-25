#include "includes.h"
#include <string.h>

void beacon_sfunc(uint8_T y[MESSAGE_BITS]) {
  build_beacon_frame();
  memcpy(y, beacon_frame, MESSAGE_BITS);
}