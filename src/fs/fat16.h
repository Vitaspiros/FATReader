#ifndef FAT16_H
#define FAT16_H
#include "../extras.h"
#include <fstream>

namespace fat16 {
    void readEBPB(EBPB* ebpb, std::ifstream& in) {
        read(&ebpb->driveNumber, in);
        in.ignore(); // Reserved
        read(&ebpb->signature, in);
        read(&ebpb->volumeId, in);
        in.read((char*)&ebpb->volumeLabel, 11);
        ebpb->volumeLabel[11] = '\0';
        in.read((char*)ebpb->systemId, 8);
        ebpb->systemId[8] = '\0';
        // I ain't reading allat
        in.ignore(448); // Boot code
        in.ignore(2); // Bootable partition signature
    }

    unsigned short getNextCluster(std::ifstream& in, BPB bpb, int cluster) {
        const int firstFATSector = bpb.reservedSectors;
        const int offset = cluster * 2; // Each cluster address is 2 bytes in FAT16
        unsigned short result;

        in.seekg(firstFATSector * bpb.bytesPerSector + offset);
        read(&result, in);

        return result;
    }

    void readDirectory(std::ifstream& in, BPB bpb, int cluster, std::vector<DirectoryEntry>& entries) {
        int position;
        if (cluster == -1) {
            const int firstFATSector = bpb.reservedSectors;
            const int FATSize = bpb.sectorsPerFAT * bpb.FATs * bpb.bytesPerSector; // FAT size in bytes
            // The root directory on FAT12/16 is immediately after the FAT
            position = firstFATSector * bpb.bytesPerSector + FATSize;
        } else {
            position = getClusterAddress(bpb, bpb.sectorsPerFAT, cluster);
        }

        in.seekg(position);
        while (true) {
            bool hasMore = readDirectoryEntry(in, entries);
            if (!hasMore) break;
        }
    }
}

#endif