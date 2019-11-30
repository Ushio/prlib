#include <vector>
#include <fstream>
#include <iostream>

int main() {
    const char *binPath = R"(C:\Users\ushio\Documents\RayTracing\PrLib\data\Verdana64falloff10_gray.png)";
    const char *hPath   = R"(C:\Users\ushio\Documents\RayTracing\PrLib\data\pr_sdf.h)";

    std::ifstream binstream(binPath, std::ios::binary);
    binstream.seekg(0, std::ios_base::end);
    std::streampos bytes = binstream.tellg();
    binstream.seekg(0, std::ios_base::beg);
    std::vector<uint8_t> bin(bytes);
    binstream.read((char *)bin.data(), bytes);

    std::ofstream hstream(hPath);
    hstream << "namespace pr {" << std::endl;
    hstream << "const uint8_t sdf_image[] = {";
    for (int i = 0; i < bin.size(); ++i) {
        if (i % 16 == 0) {
            hstream << std::endl << "    ";
        }
        hstream << std::hex << "0x" << (int)bin[i] << ", ";
    }
    hstream << "};" << std::endl;
    hstream << "}"  << std::endl;

    // !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
    //for (int c = 33; c < 127; ++c) {
    //    printf("%c", (char)c);
    //}
    //printf("\n");

    return 0;
}