# FAT32 Reader
This program can read FAT32 disks and show their contents.

# Features
- Can read directories
- Can read files

# Compilation
```
make
```
If it can't read your disk (from /dev) try running it as root.

# Thanks to
- [StackOverflow](https://stackoverflow.com/a/217605) for the string trimming algorithm
- [The official FAT specification](https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf)
- [The unofficial FAT specification at OSDev Wiki](https://wiki.osdev.org/FAT)