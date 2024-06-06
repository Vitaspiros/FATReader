#ifndef EXTRAS_H
#define EXTRAS_H

#include <ios>
#include <fstream>
#include <iostream>
#include <cstring>
#include <iomanip>
#include <math.h>
#include <sstream>
#include <string>
#include <algorithm>

#include <cmath>

// BIOS Parameter Block
struct BPB {
    unsigned char jmp[3];
    unsigned char oem[9];
    unsigned short bytesPerSector;
    unsigned char sectorsPerCluster;
    unsigned short reservedSectors;
    unsigned char FATs;
    unsigned short rootDirectoryEntries;
    unsigned short sectorsCount;
    unsigned char mediaDescriptorType;
    unsigned short sectorsPerTrack;
    unsigned short headsCount;
    unsigned int hiddenSectors;
    unsigned int sectorsCount_large;
};

// Extended BIOS Parameter Block
struct EBPB_32 {
    unsigned int sectorsPerFAT;
    unsigned short flags;
    unsigned short FATVersion;
    unsigned int rootDirCluster;
    unsigned short FSInfoSector;
    unsigned short backupBootSector;
    unsigned char driveNumber;
    unsigned char signature;
    unsigned int volumeId;
    unsigned char volumeLabel[12];
    unsigned char systemId[9];
};

// FSInfo structure (Filesystem Info)
struct FSInfo {
    unsigned int topSignature;
    unsigned int middleSignature;
    unsigned int freeClusters;
    unsigned int availableClusterStart;
    unsigned int bottomSignature;
};

// Standard 8.3 directory entry
struct DirectoryEntry {
    unsigned char filename[12];
    std::string longFilename;
    unsigned char attributes;
    unsigned char creationTimeHS;
    unsigned short creationTime;
    unsigned short creationDate;
    unsigned short lastAccessedDate;
    unsigned short firstClusterHigh;
    unsigned short lastModificationTime;
    unsigned short lastModificationDate;
    unsigned short firstClusterLow;
    unsigned int size;
};

// Long File Name Directory Entry
struct LFNDirectoryEntry {
    unsigned char order;
    unsigned char topName[10];
    unsigned char longEntryType;
    unsigned char checksum;
    unsigned char middleName[12];
    unsigned char bottomName[2];
};

struct Time {
    unsigned char hour;
    unsigned char minutes;
    unsigned char seconds;
};

struct Date {
    unsigned char year;
    unsigned char month;
    unsigned char day;
};

enum FSType {
    FAT12,
    FAT16,
    FAT32
};

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

Time convertToTime(unsigned short time) {
    Time result;
    result.hour = time & 0b1111;
    time >>= 5;
    result.minutes = time & 0b111111;
    time >>= 6;
    result.seconds = (time & 0b11111) * 2;

    return result;
}

void printTime(Time time) {
    std::cout << (int)time.hour << ":" << (int)time.minutes << ":" << (int)time.seconds << std::endl;
}

void printDate(Date date) {
    std::cout << (int)date.day << "/" << (int)date.month << "/" << (int)date.year + 1980 << std::endl;
}

Date convertToDate(unsigned short date) {
    Date result;
    result.day = date & 0b11111;
    date >>= 5;
    result.month = date & 0b1111;
    date >>= 4;
    result.year = date & 0b1111111;

    return result;
}

void assembleName(std::string& buffer, LFNDirectoryEntry entry) {
    const int nameLengths[3] = {10, 12, 4};

    for (int i = 1; i <= 10; i+=2) {
        if (entry.topName[i] == 0) break;
        buffer[(i-1)/2] = entry.topName[i];
    }

    for (int i = 1; i <= 12; i+=2) {
        if (entry.middleName[i] == 0) break;
        buffer[(i-1)/2] = entry.topName[i];
    }

    for (int i = 1; i <= 4; i+=2) {
        if (entry.bottomName[i] == 0) break;
        buffer[(i-1)/2] = entry.topName[i];
    }
}

std::string computeSizeString(int size) {
    int power = floor(log10((double)size) / log10(1024));
    float scaledSize = size / pow(1024, power);
    std::ostringstream stream;
    stream.precision(2);
    stream << std::fixed << scaledSize;
    std::string result = std::move(stream).str();

    switch (power) {
        case 1:
            result.push_back('K');
            break;
        case 2:
            result.push_back('M');
            break;
        case 3:
            result.push_back('G');
            break;
        case 4:
            result.push_back('T');
            break;
    }
    result.push_back('i');
    result.push_back('B');
    return result;
}

void printBPBInfo(BPB bpb) {
    if (bpb.jmp[0] == 0xEB && bpb.jmp[2] == 0x90)
        std::cout << "Jump instruction code found: " << std::hex << std::uppercase << (int)bpb.jmp[0] << ' ' << (int)bpb.jmp[1] << ' ' << (int)bpb.jmp[2] << std::endl; 

    std::cout << std::dec << std::nouppercase;
    
    std::cout << "OEM Identifier: " << bpb.oem << std::endl;
    std::cout << "Bytes per sector: " << bpb.bytesPerSector << std::endl;
    std::cout << "Sectors per cluster: " << (int)bpb.sectorsPerCluster << std::endl;
    std::cout << "Reserved sectors: " << bpb.reservedSectors << std::endl;
    std::cout << "Number of FATs: " << (int)bpb.FATs << std::endl;
    std::cout << "Number of root directory entries: " << bpb.rootDirectoryEntries << std::endl;
    if (bpb.sectorsCount != 0) 
        std::cout << "Number of total sectors: " << bpb.sectorsCount << std::endl;
    else
        std::cout << "Number (large) of total sectors: " << bpb.sectorsCount_large << std::endl;
    std::cout << "Media descriptor type: " << std::hex << std::uppercase << (int)bpb.mediaDescriptorType << std::dec << std::nouppercase << std::endl;
    std::cout << "Number of sectors per track: " << bpb.sectorsPerTrack << std::endl;
    std::cout << "Number of heads on the disk: " << bpb.headsCount << std::endl;
    std::cout << "Number of hidden sectors: " << bpb.hiddenSectors << std::endl;
}

void printEBPBInfo(EBPB_32 ebpb) {
    std::cout << "Sectors per FAT: " << ebpb.sectorsPerFAT << std::endl;
    std::cout << std::hex << std::uppercase << "Flags: " << ebpb.flags << std::endl;
    std::cout << std::dec << std::nouppercase << "FAT version number: " << ((ebpb.FATVersion & 0xff00) >> 8) << '.' << (ebpb.FATVersion & 0xff) << std::endl;
    std::cout << "Root directory cluster: " << ebpb.rootDirCluster << std::endl;
    std::cout << "FSInfo sector: " << ebpb.FSInfoSector << std::endl;
    std::cout << "Backup Boot Sector: " << ebpb.backupBootSector << std::endl;
    std::cout << "Drive type: " << (ebpb.driveNumber == 0 ? "Floppy" : (ebpb.driveNumber == 0x80 ? "Hard Disk" : "Other")) << std::hex << " (0x" << (int)ebpb.driveNumber << ")" << std::endl;
    if (ebpb.signature == 0x28 || ebpb.signature == 0x29) 
        std::cout << "EBPB signature found: 0x" << (int)ebpb.signature << std::endl;
    std::cout << "Volume ID: " << ebpb.volumeId << std::endl;
    std::cout << "Volume Label: " << ebpb.volumeLabel << std::endl;
    if (!strcmp((char*)ebpb.systemId, "FAT32   ")) std::cout << "System identifier correct: " << ebpb.systemId << std::endl;
}

void printFSInfo(FSInfo fsInfo) {
    std::cout << std::dec << "Top signature " << (fsInfo.topSignature == 0x41615252 ? "matches!" : "doesn't match!") << std::endl;
    std::cout << "Middle signature " << (fsInfo.middleSignature == 0x61417272 ? "matches!" : "doesn't match!") << std::endl;
    std::cout << "Last known free cluster count: ";
    if (fsInfo.freeClusters == 0xFFFFFFFF) std::cout << "N/A" << std::endl;
    else std::cout << fsInfo.freeClusters << std::endl;
    std::cout << "Start looking for available clusters at cluster number: ";
    if (fsInfo.availableClusterStart == 0xFFFFFFFF) std::cout << "N/A" << std::endl;
    else std::cout << fsInfo.availableClusterStart << std::endl;
    std::cout << "Bottom signature " << (fsInfo.bottomSignature == 0xAA550000 ? "matches!" : "doesn't match!") << std::endl;

    std::cout << std::endl;
}

void printDirectoryEntry(DirectoryEntry entry) {
    bool isDirectory = (entry.attributes & 0x10) != 0;
    std::cout << (isDirectory ? 'D' : 'F') << '\t';
    if (!entry.longFilename.empty())
        std::cout << entry.longFilename << '\t';
    else
        std::cout << entry.filename << '\t';
    Time creationTime = convertToTime(entry.creationTime);
    Date creationDate = convertToDate(entry.creationDate);

    std::cout << std::fixed << std::setfill('0') << std::setw(2) << (int)creationTime.hour << ":" << std::setw(2) << (int)creationTime.minutes << ":" << std::setw(2) << (int)creationTime.seconds << '\t';
    std::cout << std::setw(2) << (int)creationDate.day << "/" << std::setw(2) << (int)creationDate.month << "/" << std::setw(2) << (int)creationDate.year + 1980;
    if (!isDirectory) std::cout << '\t' << computeSizeString(entry.size);
    std::cout << std::endl;
}

void printDirectoryEntryInfo(DirectoryEntry entry) {
    bool isReadOnly = (entry.attributes & 0x01) != 0;
    bool isHidden = (entry.attributes & 0x02) != 0;
    bool isSystem = (entry.attributes & 0x04) != 0;
    bool isVolumeIdEntry = (entry.attributes & 0x08) != 0;
    bool isDirectory = (entry.attributes & 0x10) != 0;
    bool isArchive = (entry.attributes & 0x20) != 0;

    Date creationDate = convertToDate(entry.creationDate);
    Date lastModificationDate = convertToDate(entry.lastModificationDate);
    Date lastAccessedDate = convertToDate(entry.lastAccessedDate);

    Time creationTime = convertToTime(entry.creationTime);
    Time lastModificationTime = convertToTime(entry.lastModificationTime);

    std::cout << "Filename: ";
    if (!entry.longFilename.empty())
        std::cout << entry.longFilename << std::endl;
    else
        std::cout << entry.filename << std::endl;

    std::cout << "Type: " << (isDirectory ? "Directory" : "File") << std::endl;
    std::cout << "Attributes: " << (isReadOnly ? 'R' : ' ') << (isHidden ? 'H' : ' ') << (isSystem ? 'S' : ' ') 
                                << (isVolumeIdEntry ? 'V' : ' ') << (isDirectory ? 'D' : ' ') << (isArchive ? 'A' : ' ') << std::endl;
    std::cout << "Creation date: ";
    printDate(creationDate);
    std::cout << std::endl << "Creation time: ";
    printTime(creationTime);
    std::cout << std::endl << "Creation time (in hundrends of a second): " << (int)entry.creationTimeHS;
    std::cout << std::endl << "Last modification date: ";
    printDate(lastModificationDate);
    std::cout << std::endl << "Last modification time: ";
    printTime(lastModificationTime);
    std::cout << std::endl << "Last accessed date: ";
    printDate(lastAccessedDate);

    std::cout << std::endl << "First cluster: " << std::hex << std::uppercase << ((entry.firstClusterHigh << 16) | entry.firstClusterLow);
    std::cout << std::endl << "Size (in bytes): " << std::dec << std::nouppercase << entry.size << std::endl;
}

// Taken from https://stackoverflow.com/a/217605
// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}


#endif