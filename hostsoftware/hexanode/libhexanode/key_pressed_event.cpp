#include "key_pressed_event.hpp"
#include <sstream>

using namespace hexanode;

std::string KeyPressedEvent::str() {
  std::ostringstream oss;
  oss << "Key pressed: " << id();
  return oss.str();
}
