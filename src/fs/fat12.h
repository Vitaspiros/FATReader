#ifndef FAT12_H
#define FAT12_H

#include <fstream>
#include "../extras.h"

namespace fat12 {
    unsigned int getNextCluster(std::ifstream& in, BPB bpb, int cluster) {
        const int firstFATSector = bpb.reservedSectors;
        const int offset = floor(cluster * 1.5); // Each cluster address is 1.5 bytes (no joke) in FAT12
        unsigned short result;

        in.seekg(firstFATSector * bpb.bytesPerSector + offset);
        read(&result, in); // we read 2 bytes

        return (cluster & 1) ? result >> 4 : result & 0xFFF; // if the cluster is even, we shift it, or else we take the last 12 bits
    }
}

#endif