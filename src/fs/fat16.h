#ifndef FAT16_H
#define FAT16_H
#include "../extras.h"
#include <fstream>

namespace fat16 {
    unsigned short getNextCluster(std::ifstream& in, BPB bpb, int cluster) {
        const int firstFATSector = bpb.reservedSectors;
        const int offset = cluster * 2; // Each cluster address is 2 bytes in FAT16
        unsigned short result;

        in.seekg(firstFATSector * bpb.bytesPerSector + offset);
        read(&result, in);

        return result;
    }    
}

#endif