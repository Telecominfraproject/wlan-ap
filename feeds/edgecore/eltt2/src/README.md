# ELTT2 - Infineon Embedded Linux TPM Toolbox 2 for TPM 2.0
 

All information in this document is Copyright (c) 2014-2022, Infineon Technologies AG <br>
All rights reserved.


# 1. Welcome

Welcome to Embedded Linux TPM Toolbox 2 (ELTT2). ELTT2 is a single-file executable program intended for testing, performing diagnosis and basic state changes of the Infineon Technologies TPM 2.0.


## 1.1 Prerequisites

To build and run ELTT2 you need GCC and a Linux system capable of hosting a TPM.

ELTT2 may run on many other little-endian hardware and software configurations capable of running Linux and hosting a TPM, but this has not been tested.

ELTT2 does not support machines with a big-endian CPU.

## 1.2 Getting Started

A TPM 2.0 evaluation board can be ordered in the [Hitex Webshop](https://www.ehitex.de/evaluation-boards/infineon/2564/iridium-9670-tpm2.0-spi).


<p align="center">
  <img src="https://cloud.githubusercontent.com/assets/19730245/25651091/77a84744-2fe1-11e7-91bd-a2e39678202d.JPG" width="350"/>
</p>

In order to execute ELTT2, you need to compile it first:
1. Switch to the directory with the ELTT2 source code
2. Compile the source code by typing the following command:
    make

Due to hardware (and thus TPM) access restrictions for normal users, ELTT2 requires root (aka superuser or administrator) privileges. They can be obtained e.g. by using the 'sudo' command on Debian Linux derivates.

The Infineon [TPM 2.0 Application Note](https://www.infineon.com/dgdl/Infineon-App-Note-SLx9670-TPM2.0_Embedded_RPi_DI_SLx-AN-v01_20-EN.pdf?fileId=5546d46267c74c9a01684b96e69f5d7b) shows how the TPM device driver can be set up (e.g. for Linux Kernel 4.14).



# 2. Usage of ELTT2

## 2.1 Generic Usage

ELTT2 is operated as follows:

Call: `./eltt2 <option(s)>`

For example: `./eltt2 -g` or `./eltt2 -gc`

For getting an overview of the possible commands, run `./eltt2 -h`

Some options require the TPM to be in a specific state. This state is shown in brackets ("[]") behind each command line option in the list below:

\[-\]: none <br>
\[\*\]: the TPM platform hierarchy authorization value is not set (i.e., empty buffer) <br>
\[l\]: the required PCR bank is allocated <br>
\[u\]: started <br>

To get the TPM into the required state, call ELTT2 with the corresponding commands ("x" for a state means that whether this state is required or not depends on the actual command or the command parameters sent eventually to the TPM).

 Command line option                                 | Explanation                                       | Precondition
 ---                                                 | ---                                               | ---
 `-a [hash algorithm] <data bytes>`                  | Hash Sequence SHA-1/256/384 \[default: SHA-1\]    | \[u\]
 `-A <data bytes>`                                   | Hash Sequence SHA-256                             | \[u\]
 `-b <command bytes>`                                | Enter your own TPM command                        | \[u\]
 `-c`                                                | Read Clock                                        | \[u\]
 `-d <shutdown type>`                                | Shutdown                                          | \[u\]
 `-e [hash algorithm] <PCR index> <PCR digest>`      | PCR Extend SHA-1/256/384 \[default: SHA-1\]       | \[u\], \[l\]
 `-E <PCR index> <PCR digest>`                       | PCR Extend SHA-256                                | \[u\], \[l\]
 `-g`                                                | Get fixed capability values                       | \[u\]
 `-v`                                                | Get variable capability values                    | \[u\]
 `-G <data length>`                                  | Get Random                                        | \[u\]
 `-h`                                                | Help                                              | \[-\]
 `-l <hash algorithm>`                               | PCR Allocate SHA-1/256/384                        | \[u\], \[\*\]
 `-r [hash algorithm] <PCR index>`                   | PCR Read SHA-1/256/384 \[default: SHA-1\]         | \[u\], \[l\]
 `-R <PCR index>`                                    | PCR Read SHA-256                                  | \[u\], \[l\]
 `-s [hash algorithm] <data bytes>`                  | Hash SHA-1/256/384 \[default: SHA-1\]             | \[u\]
 `-S <data bytes>`                                   | Hash SHA-256                                      | \[u\]
 `-t <test type>`                                    | Self Test                                         | \[u\]
 `-T`                                                | Get Test Result                                   | \[u\]
 `-u <startup type>`                                 | Startup                                           | \[-\]
 `-z <PCR index>`                                    | PCR Reset                                         | \[u\]


 Additional information:

`-a`: <br>
With the "-a" command you can hash given data with the SHA-1/256/384 hash algorithm. This hash sequence sends 3 commands \[start, update, complete\] to the TPM and allows to hash an arbitrary amount of data. For example, use the following command to hash the byte sequence {0x41, 0x62, 0x43, 0x64}: <br>
`./eltt2 -a 41624364` Hash given data with SHA-1 hash algorithm. <br>
or <br>
`./eltt2 -a sha1 41624364` Hash given data with SHA-1 hash algorithm. <br>
`./eltt2 -a sha256 41624364` Hash given data with SHA-256 hash algorithm. <br>
`./eltt2 -a sha384 41624364` Hash given data with SHA-384 hash algorithm. <br>

`-A`: <br>
With the "-A" command you can hash given data with the SHA-256 hash algorithm. This hash sequence sends 3 commands \[start, update, complete\] to the TPM and allows to hash an arbitrary amount of data. For example, use the following command to hash the byte sequence {0x41, 0x62, 0x43, 0x64}: <br>
`./eltt2 -A 41624364`

`-b`: <br>
With the "-b" command you can enter your own TPM command bytes and read the TPM response. For example, use the following command to send a TPM2_Startup with startup type CLEAR to the TPM: <br>
 `./eltt2 -b 80010000000C000001440000`

`-c`: <br>
With the "-c" command you can read the clock values of the TPM. 

`-d`: <br>
With the "-d" command you can issue a TPM shutdown. It has 2 options: <br>
`./eltt2 -d` <br>
or <br>
`./eltt2 -d clear`    send a TPM2_Shutdown command with shutdown type CLEAR to the TPM. <br>
`./eltt2 -d state`    send a TPM2_Shutdown command with shutdown type STATE to the TPM. <br>

 `-e`: <br>
 With the "-e" command you can extend bytes in the selected PCR with SHA-1/256/384. To do so, you have to enter the index of PCR in hexadecimal that you like to extend and the digest you want to extend the selected PCR with. Note that you can only extend PCRs with index 0 to 16 and PCR 23 and that the digest must have a length of 20/32/48 bytes (will be padded with 0 if necessary). The TPM then builds an SHA-1/256/384 hash over the PCR data in the selected PCR and the digest you provided and writes the result back to the selected PCR. For example, use the following command to extend PCR 23 (0x17) with the byte sequence {0x41, 0x62, 0x43, 0x64, 0x00, ... (will be filled with 0x00)}: <br>
`./eltt2 -e 17 41624364` Extend bytes in PCR 23 with SHA-1. <br>
or <br>
`./eltt2 -e sha1 17 41624364` Extend bytes in PCR 23 with SHA-1. <br>
`./eltt2 -e sha256 17 41624364` Extend bytes in PCR 23 with SHA-256. <br>
`./eltt2 -e sha384 17 41624364` Extend bytes in PCR 23 with SHA-384. <br>

`-E`: <br>
With the "-E" command you can extend bytes in the selected PCR with SHA-256. To do so, you have to enter the index of PCR in hexadecimal that you like to extend and the digest you want to extend the selected PCR with. Note that you can only extend PCRs with index 0 to 16 and PCR 23 and that the digest must have a length of 32 bytes (will be padded with 0 if necessary). The TPM then builds an SHA-256 hash over the PCR data in the selected PCR and the digest you provided and writes the result back to the selected PCR. For example, use the following command to extend PCR 23 (0x17) with the byte sequence {0x41, 0x62, 0x43, 0x64, 0x00, ... (will be filled with 0x00)}: <br>
 `./eltt2 -E 17 41624364`

`-g`: <br>
With the "-g" command you can read the TPM's fixed properties.

`-v`: <br>
With the "-v" command you can read the TPM's variable properties.

`-G`: <br>
With the "-G" command you can get a given amount of random bytes. Note that you can only request a maximum amount of 32 random bytes at once. For example, use the following command to get 20 (0x14) random bytes: <br>
 `./eltt2 -G 14`

`-l`: <br>
With the "-l" command you can allocate the SHA-1/256/384 PCR bank. Take note of two things. Firstly, the command requires a platform authorization value and it is set to an empty buffer; hence the command cannot be used if the TPM platform authorization value is set (e.g., by UEFI). Secondly, when the command is executed successfully a TPM reset has to follow for it to take effect. For example, use the following command to allocate a PCR bank: <br>
`./eltt2 -l sha1` Allocate SHA-1 PCR bank. <br>
`./eltt2 -l sha256` Allocate SHA-256 PCR bank. <br>
`./eltt2 -l sha384` Allocate SHA-384 PCR bank. <br>

`-r`: <br>
With the "-r" command you can read data from a selected SHA-1/256/384 PCR. For example, use the following command to read data from PCR 23 (0x17): <br>
`./eltt2 -r 17` Read data from SHA-1 PCR 23. <br>
or <br>
`./eltt2 -r sha1 17` Read data from SHA-1 PCR 23. <br>
`./eltt2 -r sha256 17` Read data from SHA-256 PCR 23. <br>
`./eltt2 -r sha384 17` Read data from SHA-384 PCR 23. <br>

`-R`: <br>
With the "-R" command you can read data from a selected SHA-256 PCR. For example, use the following command to read data from PCR 23 (0x17): <br>
 `./eltt2 -R 17`

`-s`: <br>
With the "-s" command you can hash given data with the SHA-1/256/384 hash algorithm. This command only allows a limited amount of data to be hashed (depending on the TPM's maximum input buffer size). For example, use the following command to hash the byte sequence {0x41, 0x62, 0x43, 0x64}: <br>
`./eltt2 -s 41624364` Hash given data with SHA-1 hash algorithm. <br>
or <br>
`./eltt2 -s sha1 41624364` Hash given data with SHA-1 hash algorithm. <br>
`./eltt2 -s sha256 41624364` Hash given data with SHA-256 hash algorithm. <br>
`./eltt2 -s sha384 41624364` Hash given data with SHA-384 hash algorithm. <br>

`-S`: <br>
With the "-S" command you can hash given data with the SHA-256 hash algorithm. This command only allows a limited amount of data to be hashed (depending on the TPM input buffer size). For example, use the following command to hash the byte sequence {0x41, 0x62, 0x43, 0x64}: <br>
 `./eltt2 -S 41624364`

`-t`: <br>
With the "-t" command you can issue a TPM selftest. It has 3 options: <br>
`./eltt2 -t` <br>
or<br>
`./eltt2 -t not_full` Perform a partial TPM2_Selftest to test previously untested TPM capabilities. <br>
`./eltt2 -t full` Perform a full TPM2_Selftest to test all TPM capabilities. <br>
`./eltt2 -t incremental`  Perform a test of selected algorithms. 

`-T`: <br>
With the "-T" command you can read the results of a previously run selftest.

`-u`: <br>
With the "-u" command you can issue a TPM startup command. It has 2 options: <br>
`./eltt2 -u` <br>
or <br>
`./eltt2 -u clear` send a TPM2_Startup with startup type CLEAR to the TPM. <br>
`./eltt2 -u state` send a TPM2_Startup with startup type STATE to the TPM.

`-z`: <br>
With the "-z" command you can reset a selected PCR. Note that you can only reset PCRs 16 and 23. For example, use the following command to reset PCR 23 (0x17): <br>
 `./eltt2 -z 17`

## 2.2 Examples:

In order to work with the TPM, perform the following steps:
- Send the TPM2_Startup command: `./eltt2 -u`


# 3. If you have questions

If you have any questions or problems, please read the section "FAQ and
Troubleshooting" in this document.
In case you still have questions, contact your local Infineon
Representative.
Further information is available at <https://www.infineon.com/tpm>.


# 4. FAQ and Troubleshooting

If you encounter any error, please make sure that
- the TPM is properly connected.
- the TPM driver is loaded, i.e. check that "/dev/tpm0" exists. In case of driver loading problems (e.g. shown by "Error opening device"), reboot your system and try to load the driver again.
- ELTT2 has been started with root permissions. Please note that ELTT2 needs root permissions for all commands.
- the TPM is started. (See section 2.2 in this document on how to do this.)


The following list shows the most common errors and their solution:

The ELTT2 response is "Error opening the device.":
- You need to load a TPM driver before you can work with ELTT2.
- You need to start ELTT2 with root permissions.

The ELTT2 responds with error code 0x100.
- You need to send the TPM2_Startup command, or you did send it twice. In
  case you have not sent it yet, do so with `./eltt2 -u`.

The TPM does not change any of the permanent flags shown by sending the "-g"
command , e.g. after a force clear.
- The TPM requires a reset in order to change any of the permanent flags.
  Press the reset button or disconnect the TPM to do so.

The value of a PCR does not change after sending PCR extend or reset.
- With the application permissions you cannot modify every PCR. For more
  details, please refer to the description for the different PCR commands
  in this file.
