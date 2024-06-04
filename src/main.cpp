#include <fstream>
#include <ios>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>

#include "extras.h"

std::vector<DirectoryEntry> currentDirEntries;

template <class T>
void read(T* result, std::ifstream& in) {
    int size = sizeof(T);

    unsigned char* bytes = new unsigned char[size];
    in.read((char*)bytes, size);

    T r = 0;

    for (int i = size - 1; i >= 0; i--) {
        r <<= 8;
        r |= bytes[i];
    }
    
    *result = r;
}

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
    in.ignore(2); // Number of sectors per FAT-> This is only for FAT12 and FAT16
    read(&bpb->sectorsPerTrack, in);
    read(&bpb->headsCount, in);
    read(&bpb->hiddenSectors, in);
    read(&bpb->sectorsCount_large, in);
}

void readEBPB(EBPB* ebpb, std::ifstream& in) {
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

void readFSInfo(BPB bpb, EBPB ebpb, FSInfo* fsInfo, std::ifstream& in) {
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

unsigned int getNextCluster(BPB bpb, std::ifstream& in, int cluster) {
    const int firstFATSector = bpb.reservedSectors;
    const int offset = cluster * 4; // Each cluster address is 4 bytes in FAT32
    unsigned int result;

    in.seekg(firstFATSector * bpb.bytesPerSector + offset);
    read(&result, in);

    return result & 0x0FFFFFFF; // only 28 bits are used
}

int getClusterAddress(BPB bpb, EBPB ebpb, int cluster) {
    const int firstDataSector = bpb.reservedSectors + (bpb.FATs * ebpb.sectorsPerFAT);
    return ((cluster - 2) * bpb.sectorsPerCluster + firstDataSector) * bpb.bytesPerSector;
}

void readDirectory(BPB bpb, EBPB ebpb, std::ifstream& in, int cluster) {
    const int firstFATSector = bpb.reservedSectors;
    const int firstDataSector = firstFATSector + (bpb.FATs * ebpb.sectorsPerFAT);

    // Go to the address of the root directory's first cluster
    in.seekg(getClusterAddress(bpb, ebpb, cluster));

    int origin = in.tellg();
    int currentCluster = cluster;
    std::string longNameBuffer;

    // erase list of entries
    currentDirEntries.clear();
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
        currentDirEntries.push_back(entry);

        if ((int)in.tellg() - origin >= bpb.sectorsPerCluster * bpb.bytesPerSector) {
            // end of cluster
            // go to the next
            unsigned int nextCluster = getNextCluster(bpb, in, currentCluster);
            int address = getClusterAddress(bpb, ebpb, nextCluster);
            in.seekg(address);
            origin = address;
            currentCluster = nextCluster;
            continue;
        }
        longNameBuffer.clear();
    }
    
}

void readFile(BPB bpb, EBPB ebpb, DirectoryEntry entry, std::ifstream& in) {
    const int firstCluster = (entry.firstClusterHigh << 16) | entry.firstClusterLow;
    const int bytesPerCluster = bpb.sectorsPerCluster * bpb.bytesPerSector;
    unsigned char clusterData[bytesPerCluster+1];

    int currentCluster = firstCluster;

    while (true) {
        in.seekg(getClusterAddress(bpb, ebpb, currentCluster));
        in.read((char*)&clusterData, bytesPerCluster);
        clusterData[bytesPerCluster] = '\0';

        std::cout << clusterData;

        int nextCluster = getNextCluster(bpb, in, currentCluster);
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

int main(int argc, const char** argv) {
    BPB bpb;
    EBPB ebpb;
    FSInfo fsInfo;

    // check argument count
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <input drive>" << std::endl;
        return -1;
    }

    // open the file
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "could not open drive!" << std::endl;
        return -1; 
    } else
        std::cout << "Drive opened." << std::endl;

    readBPB(&bpb, in);
    readEBPB(&ebpb, in);
    readFSInfo(bpb, ebpb, &fsInfo, in);

    readDirectory(bpb, ebpb, in, ebpb.rootDirCluster);

    while (true) {
        std::cout << std::endl << "> ";
        
        std::string command;
        std::getline(std::cin, command, '\n');

        if (command == "fsinfo") {
            // Print the information
            std::cout << "**** Info for BPB (BIOS Parameter Block) ****" << std::endl << std::endl;
            printBPBInfo(bpb);

            std::cout << std::endl << "**** Info for EBPB (Extended BIOS Parameter Block) ****" << std::endl;
            printEBPBInfo(ebpb);
            std::cout << std::endl << "**** Info for FSInfo structure (Filesystem info) ****" << std::endl;
            printFSInfo(fsInfo);
        } else if (command == "fileinfo") {
            std::string filename;

            std::cout << "Filename: ";
            std::getline(std::cin, filename, '\n');

            bool found = false;
            for (int i = 0; i < currentDirEntries.size(); i++) {
                bool hasLongFilename = !currentDirEntries.at(i).longFilename.empty();
                std::string entryFilename;
                if (hasLongFilename)
                    entryFilename = currentDirEntries.at(i).longFilename;
                else
                    entryFilename = std::string((char*)(currentDirEntries.at(i).filename));
                rtrim(entryFilename);
                if (entryFilename == filename) {
                    printDirectoryEntryInfo(currentDirEntries.at(i));
                    found = true;
                    break;
                }
            }

            if (!found) std::cout << "File " << filename << "was not found." << std::endl;
        } else if (command == "ls") {
            for (int i = 0; i < currentDirEntries.size(); i++) {
                printDirectoryEntry(currentDirEntries.at(i));
            }
        } else if (command == "exit" || command == "quit") {
            break;
        } else {
            bool found = false;
            for (int i = 0; i < currentDirEntries.size(); i++) {
                bool hasLongFilename = !currentDirEntries.at(i).longFilename.empty();
                std::string entryFilename;
                if (hasLongFilename)
                    entryFilename = currentDirEntries.at(i).longFilename;
                else
                    entryFilename = std::string((char*)(currentDirEntries.at(i).filename));
                rtrim(entryFilename);
                if (entryFilename == command) {
                    bool isDirectory = (currentDirEntries.at(i).attributes & 0x10) != 0;
                    if (!isDirectory)
                        readFile(bpb, ebpb, currentDirEntries.at(i), in);
                    else {
                        const int firstCluster = (currentDirEntries.at(i).firstClusterHigh << 16) | currentDirEntries.at(i).firstClusterLow;
                        readDirectory(bpb, ebpb, in, firstCluster);
                        std::cout << "Switched to directory " << entryFilename << std::endl;
                    }
                    found = true;
                }
            }
            if (!found) std::cout << "Unknown command." << std::endl;
        }
    }

    in.close();
    return 0;
}