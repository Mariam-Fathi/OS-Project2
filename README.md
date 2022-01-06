# OS-Project2

pzip OS project


Description

For this project, we will implement a parallel version of zip using threads.
We 'll use the run-length encoding (RLE) as the basic technique.

RLE is quite simple: when you encounter n characters of the same type in a row, the compression tool (pzip)
will turn that into the number n and a single instance of the character.
Thus, if we had a file with the following contents: aaaaaaaaaabbbb
The tool would turn it into: 10a4b

Here, we will write out a 4-byte integer in binary format followed by the single character in ASCII. Thus, a compressed file will consist of some
number of 5-byte entries, each of which is comprised of a 4-byte integer and the single character. To write out an integer in binary format (not ASCII), you should use fwrite(). For pzip, all output should be written to standard output (the stdout file strem).

• To determine the number of threads to create. On Linux, the determination of the number of
threads may refer to some interfaces like get_nprocs() and get_nprocs_conf();. Then, we are required to create an appropriate number of threads
to match the number of CPUs available on whichever system our program is running.
