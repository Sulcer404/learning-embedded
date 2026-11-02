#include <bitset>
#include <cstdint>
#include <iostream>

int main() {
    std::cout << "Binary counter (0–15)\n\n";

    for (std::uint8_t n = 0; n < 16; ++n) {
        std::cout << static_cast<unsigned>(n) << " = "
                  << std::bitset<4>(n) << '\n';
    }

    return 0;
}
