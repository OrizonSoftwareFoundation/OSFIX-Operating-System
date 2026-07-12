# OSFIX Operating System (OSFIX) 

Created by the Orizon Software Foundation, the OSFIX Operating System is a new OS that's main goal is to run multiple executable types, such as Microsoft Windows's EXE file type, and Linux's ELF file type, 
therefore fixing the issue of having to dualboot Windows and Linux.

This operating system uses a modular build system made in the GNU Makefile Language, a fast and complex build language made specifically for large projects. More information can be found here: https://www.gnu.org/software/make/

Currently, the OSFIX Operating System uses Flanterm for its terminal, as it is feature rich and includes better scrolling and color handling. 
This OS also features the Limine Bootloader, because of its extensive features and ease of use. I personally support its goals and achievements, which is why i've included it in this project.
 
---

## Building the OS

To build this OS, simply run the command below in the project's root directory, which will compile the project, and run the .img file generated:
```
make && make run
```
To generate full documentation for the OS, simply run:
```
make docs
```

The required dependencies are as follows:

- **Make** *(Buildsystem package.)* 
- **CC** *(Standard C Compiler for UNix-like systems.)*
- **QEMU-Full** *(can be substituted for qemu-system-x86_64.)*
- **Parted** *(For .img partitioning.)*
- **DOSFSTools** *(We don't even know why.)*
- **MTools** *(For copying things to the .img file.)*
- **NASM** *(Assembly Assembler.)*
- **cpio** *(Needed for the initramfs)*

**Note**: The method of installing these packages may differ by distribution and operating system. Please refer to the documentation for your distribution and/or OS.


---

# Contributing
Thank you very much for contributing to OSFIX!
You can contribute to this project by creating a pull request with your changes;
A maintainer will review your changes and either accept or deny them with details depending on the use case.

---

## License(s)

Most of this project is licensed and shipped under EUPL 1.2 as seen in the LICENSE file, but some code snippets/files may be subject to different licenses. 
Please refer to the file contents and the disclaimer at the top of the file for more information.


