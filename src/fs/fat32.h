#ifndef FAT32_H
#define FAT32_H

#include "../extras.h"

namespace fat32 {
    unsigned int getNextCluster(BPB bpb, std::ifstream& in, int cluster) {
        const int firstFATSector = bpb.reservedSectors;
        const int offset = cluster * 4; // Each cluster address is 4 bytes in FAT32
        unsigned int result;

        in.seekg(firstFATSector * bpb.bytesPerSector + offset);
        read(&result, in);

        return result & 0x0FFFFFFF; // only 28 bits are used
    }

    void readEBPB(EBPB_32* ebpb, std::ifstream& in) {
        read(&ebpb->sectorsPerFAT, in);
        read(&ebpb->flags, in);
        read(&ebpb->FATVersion, in);
        read(&ebpb->rootDirCluster, in);
        read(&ebpb->FSInfoSector, in);
        read(&ebpb->backupBootSector, in);
        in.ignore(12); // Reserved
        read(&ebpb->driveNumber, in);
        in.ignore(1); // Reserved
        read(&ebpb->signature, in);
        read(&ebpb->volumeId, in);
        in.read((char*)&ebpb->volumeLabel, 11);
        ebpb->volumeLabel[11] = '\0';
        in.read((char*)&ebpb->systemId, 8);
        ebpb->systemId[8] = '\0';
        in.ignore(420); // Boot code
        in.ignore(2); // Bootable partition signature (0xAA55)
    }

    void readFSInfo(BPB bpb, EBPB_32 ebpb, FSInfo* fsInfo, std::ifstream& in) {
        // FSInfo
        in.seekg(ebpb.FSInfoSector * bpb.bytesPerSector); // seek to FSInfo start location
        read(&fsInfo->topSignature, in);
        in.ignore(480); // Reserved
        read(&fsInfo->middleSignature, in);
        read(&fsInfo->freeClusters, in);
        read(&fsInfo->availableClusterStart, in);
        in.ignore(12); // Reserved
        read(&fsInfo->bottomSignature, in);
    }
}

#endif