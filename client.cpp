#include <iostream>

#include "hadev.cpp"

int main() {
  HadevClient hadev;
  hadev.Write(0, "apple");

  std::cout << hadev.Read(0) << std::endl;
}
