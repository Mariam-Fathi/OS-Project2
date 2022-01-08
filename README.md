# OS-Project2

pzip OS project


Description

For this project, we will implement a parallel version of zip using threads.
We 'll use the run-length encoding (RLE) as the basic technique.

RLE is quite simple: when we encounter n characters of the same type in a row, the compression tool will turn that into the number n and a single instance of the character.

Thus, if we had a file with the following contents: aaaaaaaaaabbbb

The tool would turn it into: 10a4b

Here, we will write out a 4-byte integer in binary format followed by the single character in ASCII. Thus, a compressed file will consist of some
number of 5-byte entries, each of which is comprised of a 4-byte integer and the single character.

• To determine the number of threads to create. On Linux, the determination of the number of
threads may refer to some interfaces like get_nprocs() and get_nprocs_conf();. Then, we are required to create an appropriate number of threads
to match the number of CPUs available on whichever system our program is running.

•The mmap() function is used for mapping between a process address space and either files or devices.

When a file is mapped to a process address space, the file can be accessed like an array in the program.

map = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, file, 0);

PROT_READ: Pages may be read.

MAP_SHARED:   Share this mapping.  Updates to the mapping are visible to
              other processes mapping the same region, and (in the case
              of file-backed mappings) are carried through to the
              underlying file.  (To precisely control when updates are
              carried through to the underlying file requires the use of
              msync(2).)

# tests


![pzip test](https://user-images.githubusercontent.com/66404704/148614492-681d2449-b4b8-48e0-b73c-f078d1b616e6.jpeg)

