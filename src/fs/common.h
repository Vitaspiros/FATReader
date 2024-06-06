#ifndef COMMON_FS_H
#define COMMON_FS_H
#include "../extras.h"
#include "fat32.h"
#include <fstream>
#include <vector>

void readBPB(BPB* bpb, std::ifstream& in) {
    in.read((char*)bpb->jmp, 3);
    in.read((char*)bpb->oem, 8);
    bpb->oem[8] = '\0';
    read(&bpb->bytesPerSector, in);
    read(&bpb->sectorsPerCluster, in);
    read(&bpb->reservedSectors, in);
    read(&bpb->FATs, in);
    read(&bpb->rootDirectoryEntries, in);
    read(&bpb->sectorsCount, in);
    read(&bpb->mediaDescriptorType, in);
    read(&bpb->sectorsPerFAT, in); // Number of sectors per FAT - This is only for FAT12 and FAT16
    read(&bpb->sectorsPerTrack, in);
    read(&bpb->headsCount, in);
    read(&bpb->hiddenSectors, in);
    read(&bpb->sectorsCount_large, in);
}

int getClusterAddress(BPB bpb, unsigned int sectorsPerFAT, int cluster) {
    const int firstDataSector = bpb.reservedSectors + (bpb.FATs * sectorsPerFAT);
    return ((cluster - 2) * bpb.sectorsPerCluster + firstDataSector) * bpb.bytesPerSector;
}

int getNextCluster(FSType fsType, BPB bpb, std::ifstream& in, int cluster) {
    switch (fsType) {
        case FAT32:
            return fat32::getNextCluster(bpb, in, cluster);
        default: return 0;
    }
}

void readDirectory(FSType fsType, BPB bpb, unsigned int sectorsPerFAT, std::ifstream& in, int cluster, std::vector<DirectoryEntry>& result) {
    const int firstFATSector = bpb.reservedSectors;
    const int firstDataSector = firstFATSector + (bpb.FATs * sectorsPerFAT);

    // Go to the address of the root directory's first cluster
    in.seekg(getClusterAddress(bpb, sectorsPerFAT, cluster));

    int origin = in.tellg();
    int currentCluster = cluster;
    std::string longNameBuffer;

    // erase list of entries
    result.clear();
    while (true) {
        unsigned char firstByte, eleventhByte;
        read(&firstByte, in);
        in.seekg((int)in.tellg() + 10);
        read(&eleventhByte, in);
        in.seekg((int)in.tellg() - 12); // go back one byte, so we can read it again

        // long filename entry
        if (eleventhByte == 0x0F) {
            LFNDirectoryEntry entry;
            read(&entry.order, in);
            in.read((char*)&entry.topName, 10);
            in.ignore(); // the eleventh byte
            read(&entry.longEntryType, in);
            read(&entry.checksum, in);
            in.read((char*)&entry.middleName, 12);
            in.ignore(2); // always zero
            in.read((char*)&entry.bottomName, 4);

            assembleName(longNameBuffer, entry);
            continue;
        }
        // test the first byte
        // end of directory
        if (firstByte == 0x00) break;
        // unused entry
        if (firstByte == 0xE5) {
            in.seekg((int)in.tellg() + 32);
            continue;
        }

        DirectoryEntry entry;
        in.read((char*)&entry.filename, 11);
        entry.filename[11] = '\0';
        read(&entry.attributes, in);
        in.ignore(); // Reserved
        read(&entry.creationTimeHS, in);
        read(&entry.creationTime, in);
        read(&entry.creationDate, in);
        read(&entry.lastAccessedDate, in);
        read(&entry.firstClusterHigh, in);
        read(&entry.lastModificationTime, in);
        read(&entry.lastModificationDate, in);
        read(&entry.firstClusterLow, in);
        read(&entry.size, in);

        if (!longNameBuffer.empty()) {
            entry.longFilename = longNameBuffer;
        }
        result.push_back(entry);

        if ((int)in.tellg() - origin >= bpb.sectorsPerCluster * bpb.bytesPerSector) {
            // end of cluster
            // go to the next
            unsigned int nextCluster = getNextCluster(fsType, bpb, in, cluster);

            int address = getClusterAddress(bpb, sectorsPerFAT, nextCluster);
            in.seekg(address);
            origin = address;
            currentCluster = nextCluster;
            continue;
        }
        longNameBuffer.clear();
    }
    
}

void readFile(FSType fsType, BPB bpb, unsigned int sectorsPerFAT, DirectoryEntry entry, std::ifstream& in) {
    const int firstCluster = (entry.firstClusterHigh << 16) | entry.firstClusterLow;
    const int bytesPerCluster = bpb.sectorsPerCluster * bpb.bytesPerSector;
    unsigned char clusterData[bytesPerCluster+1];

    int currentCluster = firstCluster;

    while (true) {
        in.seekg(getClusterAddress(bpb, sectorsPerFAT, currentCluster));
        in.read((char*)&clusterData, bytesPerCluster);
        clusterData[bytesPerCluster] = '\0';

        std::cout << clusterData;

        int nextCluster = getNextCluster(fsType, bpb, in, currentCluster);
        //std::cout << std::endl << "next cluster" << std::endl;
        if (nextCluster >= 0x0FFFFFF8) {
            std::cout << std::endl << "end of file" << std::endl;
            break;
        } else if (nextCluster == 0x0FFFFFF7) {
            std::cerr << "bad cluster - stopping" << std::endl;
            break;
        }
        currentCluster = nextCluster;
    }
}

FSType detectFSType(BPB bpb) {
    int totalSectors = (bpb.sectorsCount == 0) ? bpb.sectorsCount_large : bpb.sectorsCount;
    int sectorsPerFAT = bpb.sectorsPerFAT;
    if (sectorsPerFAT == 0) return FAT32; // Only FAT16 and FAT12 have this value set
    int rootDirectorySize = ((bpb.rootDirectoryEntries * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector; // Root directory size in sectors

    int dataSectors = totalSectors - (bpb.reservedSectors + (bpb.FATs * sectorsPerFAT) + rootDirectorySize);
    int totalClusters = dataSectors / bpb.sectorsPerCluster;

    if (totalClusters < 4085) return FAT12;
    if (totalClusters < 65525) return FAT16;
    else return FAT32;
}

#endif