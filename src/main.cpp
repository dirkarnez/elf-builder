#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

constexpr uint64_t virtualStartAddress = 0x400000;
constexpr uint64_t dataVirtualStartAddress = 0x600000;
constexpr uint64_t alignment = 0x200000;

class Builder {
public:
    void WriteBytes(const std::vector<uint8_t>& bytes) {
        output.insert(output.end(), bytes.begin(), bytes.end());
    }

    void WriteValue(int size, uint64_t value) {
        std::vector<uint8_t> buf(size);
        for (int i = 0; i < size; i++) {
            buf[i] = static_cast<uint8_t>(value >> (i * 8));
        }
        WriteBytes(buf);
    }

    std::vector<uint8_t> GetOutput() { return output; }

private:
    std::vector<uint8_t> output;
};

std::vector<uint8_t> buildELF(const std::vector<uint8_t>& textSection,
                             const std::vector<uint8_t>& dataSection) {
    uint64_t textSize = textSection.size();
    uint64_t textOffset = 0x40 + (2 * 0x38);

    Builder o;

    // Build ELF Header
    o.WriteBytes({0x7f, 0x45, 0x4c, 0x46});  // ELF magic value
    o.WriteBytes({0x02});                    // 64-bit executable
    o.WriteBytes({0x01});                    // Little endian
    o.WriteBytes({0x01});                    // ELF version
    o.WriteBytes({0x00});                    // Target OS ABI
    o.WriteBytes({0x00});                    // Further specify ABI version
    o.WriteBytes({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});  // Unused bytes
    o.WriteBytes({0x02, 0x00});              // Executable type
    o.WriteBytes({0x3e, 0x00});              // x86-64 target architecture
    o.WriteBytes({0x01, 0x00, 0x00, 0x00});  // ELF version
    o.WriteValue(8, virtualStartAddress + textOffset);
    o.WriteBytes({0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});  // Offset from file to program header
    o.WriteBytes({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});  // Start of section header table
    o.WriteBytes({0x00, 0x00, 0x00, 0x00});   // Flags
    o.WriteBytes({0x40, 0x00});               // Size of this header
    o.WriteBytes({0x38, 0x00});               // Size of a program header table entry
    o.WriteBytes({0x02, 0x00});               // Length of sections: data and text
    o.WriteBytes({0x00, 0x00});               // Size of section header, which we aren't using
    o.WriteBytes({0x00, 0x00});               // Number of entries section header
    o.WriteBytes({0x00, 0x00});               // Index of section header table entry

    // Build Program Header
    // Text Segment
    o.WriteBytes({0x01, 0x00, 0x00, 0x00});   // PT_LOAD, loadable segment
    o.WriteBytes({0x05, 0x00, 0x00, 0x00});   // Flags: 0x4 executable, 0x2 write, 0x1 read
    o.WriteValue(8, 0);                       // Offset from the beginning of the file
    o.WriteValue(8, virtualStartAddress);
    o.WriteValue(8, virtualStartAddress);     // Physical address, irrelevant on linux
    o.WriteValue(8, textSize);                // Number of bytes in file image of segment
    o.WriteValue(8, textSize);                // Number of bytes in memory image of segment
    o.WriteValue(8, alignment);

    uint64_t dataSize = dataSection.size();
    uint64_t dataOffset = textOffset + textSize;
    uint64_t dataVirtualAddress = dataVirtualStartAddress + dataOffset;

    // Build Program Header
    // Data Segment
    o.WriteBytes({0x01, 0x00, 0x00, 0x00});   // PT_LOAD, loadable segment
    o.WriteBytes({0x07, 0x00, 0x00, 0x00});   // Flags: 0x4 executable, 0x2 write, 0x1 read
    o.WriteValue(8, dataOffset);              // Offset address
    o.WriteValue(8, dataVirtualAddress);      // Virtual address
    o.WriteValue(8, dataVirtualAddress);      // Physical address
    o.WriteValue(8, dataSize);                // Number of bytes in file image
    o.WriteValue(8, dataSize);                // Number of bytes in memory image
    o.WriteValue(8, alignment);

    // Output the text segment
    o.WriteBytes(textSection);
    // Output the data segment
    o.WriteBytes(dataSection);
    return o.GetOutput();
}

int main(int argc, char* argv[]) {
    const char* outputBinaryName = "tiny-x64";
    const char* wordToOutput = "Hello World, this is my tiny executable";

    // Data section with word in it
    std::vector<uint8_t> dataSection(std::strlen(wordToOutput));
    memcpy(dataSection.data(), wordToOutput, dataSection.size());
    uint8_t wordLen = static_cast<uint8_t>(dataSection.size());

    // https://defuse.ca/online-x86-assembler.htm#disassembly
    std::vector<uint8_t> textSection = {
		// Sys write
		0x48, 0xC7, 0xC0, 0x04, 0x00, 0x00, 0x00, // mov rax, 0x04
		0x48, 0xC7, 0xC3, 0x01, 0x00, 0x00, 0x00, // mov rbx, 0x01
		0x48, 0xC7, 0xC2, wordLen, 0x00, 0x00, 0x00, // mov rdx, <wordLen>

		0x48, 0xC7, 0xC1, 0xDA, 0x00, 0x60, 0x00, // mov rdx, 0x6000da (HARD CODED at the moment)

		0xcd, 0x80, // int 0x80

		// Sys exit
		0xb8, 0x01, 0x00, 0x00, 0x00, // mov rax, 0x1
		0xbb, 0x00, 0x00, 0x00, 0x00, // mov rbx, 0x0
		0xcd, 0x80, // int 0x80
    };

    std::vector<uint8_t> data = buildELF(textSection, dataSection);
    std::ofstream outfile(outputBinaryName, std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(data.data()), data.size());
    outfile.close();
    std::cout << "Wrote binary to " << outputBinaryName << std::endl;
    return 0;
}