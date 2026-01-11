#include <iostream>
#include "AbbyCrypt.hpp"

int main() {
    std::string serial = Abby::AbbyCrypt::getHardwareSerial();
    
    std::cout << "=== Abby Device Key ===" << std::endl;
    std::cout << "Hardware Serial: " << serial << std::endl;
    std::cout << std::endl;
    std::cout << "Use this key when encrypting files for this device:" << std::endl;
    std::cout << "./encrypt_util input.mp3 output.pira " << serial << std::endl;
    
    return 0;
}
