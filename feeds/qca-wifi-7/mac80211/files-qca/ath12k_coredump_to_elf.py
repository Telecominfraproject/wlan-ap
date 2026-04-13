"""
Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
"""

import struct
import uuid
import argparse
import sys
import time
from ctypes import *
import io
import ctypes

CNSS_NUM_META_INFO_SEGMENTS = 1
CNSS_RAMDUMP_MAGIC = 0x574C414E
CNSS_RAMDUMP_VERSION = 0

#enum ath11k_fw_crash_dump_type
ATH11K_FW_CRASH_PAGING_DATA = 0
ATH11K_FW_CRASH_RDDM_DATA = 1
ATH11K_FW_REMOTE_MEM_DATA = 2
ATH11K_FW_CRASH_DYN_PAGING = 3
ATH11K_FW_M3_DUMP_DATA = 4
ATH11K_FW_QDSS_DATA = 5
ATH11K_FW_CRASH_DUMP_MAX = 6

#ELF hdr value definitions
EI_MAG0 = 0
EI_MAG1 = 1
EI_MAG2 = 2
EI_MAG3 = 3
EI_NIDENT = 16
EI_CLASS = 4
EI_DATA = 5
EI_VERSION = 6
EI_OSABI = 7
ELFMAG0 = 0x7f
ELFMAG1 = 'E'
ELFMAG2 = 'L'
ELFMAG3 = 'F'
ELFMAG = "\177ELF"
SELFMAG = 4
ELFCLASS32 = 1
ELFDATA2LSB = 1
EV_CURRENT = 1
ELFOSABI_NONE = 0
ET_CORE = 4
EM_NONE = 0
PT_LOAD = 1
PF_R = 0x4
PF_W = 0x2
PF_X = 0x1


class Elf_Ehdr(Structure):
    _fields_ = [('e_ident', c_ubyte*EI_NIDENT),
                ('e_type', c_uint16),
                ('e_machine', c_uint16),
                ('e_version', c_uint32),
                ('e_entry', c_uint32),
                ('e_phoff', c_uint32),
                ('e_shoff', c_uint32),
                ('e_flags', c_uint32),
		('e_ehsize', c_uint16),
		('e_phentsize', c_uint16),
		('e_phnum', c_uint16),
		('e_shentsize', c_uint16),
		('e_shnum', c_uint16),
		('e_shstrndx', c_uint16)]

class Elf_Phdr(Structure):
    _fields_ = [('p_type', c_uint32),
                ('p_offset', c_uint32),
                ('p_vaddr', c_uint32),
                ('p_paddr', c_uint32),
		('p_filesz', c_uint32),
		('p_memsz', c_uint32),
		('p_flags', c_uint32),
		('p_align', c_uint32)]

class CnssDumpEntry(Structure):
	_fields_ = [('type', c_uint32),
		    ('entry_start', c_uint32),
		    ('entry_num', c_uint32 )]

class CnssDumpMetaInfo(Structure):
	_fields_ = [('magic', c_uint32),
		    ('version', c_uint32),
		    ('chipset', c_uint32),
            	    ('total_entries', c_uint32),
		    ('entry', CnssDumpEntry * 6)]

	def print_meta_info(self):
		print('META INFO DATA: \n')
		print('Magic: %d' % (self.magic))
		print('Version: %d' % (self.version))
		print('Chipset: 0x%08x' % (self.chipset))
		print('Num_entries: %d \n' % (self.total_entries))
		print("PAGING SEGMENTS INFO:")
		print('Entry Type: %d' % (self.entry[ATH11K_FW_CRASH_PAGING_DATA].type))
		print('Entry Start: %d' % (self.entry[ATH11K_FW_CRASH_PAGING_DATA].entry_start))
		print('Entry Num: %d \n' % (self.entry[ATH11K_FW_CRASH_PAGING_DATA].entry_num))
		print("RDDM SEGMENTS INFO:")
		print('Entry Type: %d' % (self.entry[ATH11K_FW_CRASH_RDDM_DATA].type))
		print('Entry Start: %d' % (self.entry[ATH11K_FW_CRASH_RDDM_DATA].entry_start))
		print('Entry Num: %d \n' % (self.entry[ATH11K_FW_CRASH_RDDM_DATA].entry_num))
		print("REMOTE MEM SEGMENTS INFO:")
		print('Entry Type: %d' % (self.entry[ATH11K_FW_REMOTE_MEM_DATA].type))
		print('Entry Start: %d' % (self.entry[ATH11K_FW_REMOTE_MEM_DATA].entry_start))
		print('Entry Num: %d\n' % (self.entry[ATH11K_FW_REMOTE_MEM_DATA].entry_num))
		print("ATH11K_FW_CRASH_DYN_PAGING:")
		print('Entry Type: %d' % (self.entry[ATH11K_FW_CRASH_DYN_PAGING].type))
		print('Entry Start: %d' % (self.entry[ATH11K_FW_CRASH_DYN_PAGING].entry_start))
		print('Entry Num: %d\n' % (self.entry[ATH11K_FW_CRASH_DYN_PAGING].entry_num))

class Ath11kDumpSegment:

    def __init__(self):
        self.addr = None
        self.vaddr = None
        self.len = None
        self.type = None

class Ath11kFwCrashDump:
    df_magic = 'ATH12K-FW-DUMP\0\0'

    def __init__(self):
        self.segments = None
        self.is_qdss = 0
        self.valid_rddm_seg = 0
        self.is_pcss = 0

    def parse(self, filename):
        f = open(filename, 'rb')
        self.buf = f.read()
        f.close()

        offset = 0

        if len(self.buf) < 4096:
            raise Exception('crash dump header too short: %d' % len(self.buf))

        # check magic
        fmt = '<%ds' % (len(self.df_magic))
        (df_magic,) = struct.unpack_from(fmt, self.buf, offset)
        offset = offset + len(self.df_magic)
        self.df_magic = self.df_magic.encode('utf-8')
        if df_magic != self.df_magic:
            raise Exception('Invalid magic: %s' % df_magic)

        # length & version
        (self.len, self.version) = struct.unpack_from("<ii", self.buf,
                                                         offset)
        offset = offset + 4 + 4

        if self.len != 4096:
            raise Exception('Invalid length: %d vs %d' % (len(self.buf), self.len))

        # chip_id, qrtr_id
        (self.chip_id, self.qrtr_id) = struct.unpack_from("<2i", self.buf,
                                                               offset)
        offset = offset + 2 * 4

        #bus_id
        (self.bus_id,) = struct.unpack_from("<B", self.buf,
                                                    offset)
        offset = offset + 1

        # uuid
        (tmp_guid,) = struct.unpack_from("<16s", self.buf, offset)
        offset = offset + 16

        self.guid = uuid.UUID(bytes_le=tmp_guid)

        # tv_sec and tv_nsec
        (self.tv_sec, self.tv_nsec) = struct.unpack_from("<QQ", self.buf, offset)
        offset = offset + 8 + 8

        # skip unused
        offset = offset + 8

        (self.num_seg,) = struct.unpack_from("<i", self.buf,
                                                          offset)
        offset = offset + 4

        (self.seg_size,) = struct.unpack_from("<i", self.buf,
                                                        offset)
        offset = offset + 4

        self.segments = []
        for i in range(0, self.num_seg):
            seg = Ath11kDumpSegment()

            if (self.seg_size == 16) :
                (seg.addr,
                seg.vaddr,
                seg.len,
                seg.type) = struct.unpack_from('<iiii', self.buf, offset)

                offset += 4 * 4
            else :
                (seg.addr,) = struct.unpack_from("<Q", self.buf, offset)
                offset = offset + 8

                (seg.vaddr,) = struct.unpack_from("<Q", self.buf, offset)
                offset = offset + 8

                (seg.len,
                seg.type) = struct.unpack_from('<ii', self.buf, offset)

                offset += 2 * 4

            if (seg.type == ATH11K_FW_QDSS_DATA):
                self.is_qdss = 1
            elif (seg.type == ATH11K_FW_M3_DUMP_DATA):
                self.is_pcss = 1
            else :
                self.valid_rddm_seg += 1

            self.segments.append(seg)

        self.seg_start = offset


    def get_meta_info(self):
        meta_info = CnssDumpMetaInfo()
        i = 0

        meta_info.magic = CNSS_RAMDUMP_MAGIC
        meta_info.version = CNSS_RAMDUMP_VERSION
        meta_info.chipset = self.chip_id
        meta_info.total_entries = ATH11K_FW_CRASH_DUMP_MAX

        for fw_seg in self.segments:
            if (fw_seg.type >= ATH11K_FW_CRASH_DUMP_MAX):
                i+=1
                continue

            if ((meta_info.entry[fw_seg.type].entry_start) == 0) :
            	meta_info.entry[fw_seg.type].type = fw_seg.type
            	meta_info.entry[fw_seg.type].entry_start = i + 1

            i += 1
            meta_info.entry[fw_seg.type].entry_num += 1

        return meta_info

    def generate_bin(self):
        fp = open("paging.bin", 'wb')
        fr = open("remote.bin", 'wb')
        fw = open("fwdump.bin", 'wb')
        fd = open("dyn_paging.bin", 'wb')

        if (self.is_qdss):
            fq = open("qdss.bin", 'wb')
        if (self.is_pcss):
            fm = open("m3_dump.bin",'wb')

        off = 4096

        for s in self.segments:
            data = self.buf[off:off + s.len]
            if (s.type == ATH11K_FW_CRASH_PAGING_DATA):
                fp.write(data)
            if (s.type == ATH11K_FW_CRASH_RDDM_DATA):
                fw.write(data)
            if (s.type == ATH11K_FW_REMOTE_MEM_DATA):
                fr.write(data)
            if (s.type == ATH11K_FW_QDSS_DATA):
                fq.write(data)
            if (s.type == ATH11K_FW_M3_DUMP_DATA):
                fm.write(data)
            if (s.type == ATH11K_FW_CRASH_DYN_PAGING):
                fd.write(data)

            off = off+s.len

        fp.close()
        fr.close()
        fw.close()
        fd.close()

        if (self.is_qdss):
            fq.close()
        if (self.is_pcss):
            fm.close()

    def generate_coredump(self):
        fp = open("ramdump_QCN9224", 'wb')

        if (self.is_qdss):
            fq = open("qdss.bin", 'wb')

        buf = io.BytesIO()

        ehdr = Elf_Ehdr()
        ctypes.memset(ctypes.addressof(ehdr), 0, ctypes.sizeof(ehdr))

        ehdr.e_ident[EI_MAG0] = ELFMAG0
        ctypes.memmove(ctypes.addressof(ehdr.e_ident)+EI_MAG1, ELFMAG1, 1)
        ctypes.memmove(ctypes.addressof(ehdr.e_ident)+EI_MAG2, ELFMAG2, 1)
        ctypes.memmove(ctypes.addressof(ehdr.e_ident)+EI_MAG3, ELFMAG3, 1)
        ehdr.e_ident[EI_CLASS] = ELFCLASS32
        ehdr.e_ident[EI_DATA] = ELFDATA2LSB
        ehdr.e_ident[EI_VERSION] = EV_CURRENT
        ehdr.e_ident[EI_OSABI] = ELFOSABI_NONE
        ehdr.e_type = ET_CORE
        ehdr.e_machine = EM_NONE
        ehdr.e_version = EV_CURRENT
        ehdr.e_phoff = sizeof(Elf_Ehdr)
        ehdr.e_ehsize = sizeof(Elf_Ehdr)
        ehdr.e_phentsize = sizeof(Elf_Phdr)
        ehdr.e_phnum = self.valid_rddm_seg + CNSS_NUM_META_INFO_SEGMENTS

        #write ehdr
        buf.write(ehdr)

        meta_info = self.get_meta_info()

        phdr = Elf_Phdr()

        offset = ehdr.e_phoff + sizeof(phdr) * ehdr.e_phnum;
        seg_offset = offset

        #phdr for meta_info
        ctypes.memset(ctypes.addressof(phdr), 0, ctypes.sizeof(phdr))
        phdr.p_type = PT_LOAD;
        phdr.p_offset = offset;
        phdr.p_filesz = 4096;
        phdr.p_memsz = 4096;
        phdr.p_flags = PF_R | PF_W | PF_X;
        phdr.p_align = 0;

        offset += phdr.p_filesz;
        buf.write(phdr)

        #phdr for valid rddm segments
        for fw_seg in self.segments:
                if (fw_seg.type < 0 or fw_seg.type > 4) :
                        continue
                elif (fw_seg.type == ATH11K_FW_QDSS_DATA) :
                        continue

                phdr = Elf_Phdr()
                ctypes.memset(ctypes.addressof(phdr), 0, ctypes.sizeof(phdr))
                phdr.p_type = PT_LOAD
                phdr.p_offset = offset
                phdr.p_vaddr = fw_seg.addr
                phdr.p_paddr = fw_seg.addr
                phdr.p_filesz = fw_seg.len
                phdr.p_memsz = fw_seg.len
                phdr.p_flags = PF_R | PF_W | PF_X
                phdr.p_align = 0

                offset += phdr.p_filesz
                buf.write(phdr)

        #write meta_info as first seg data
        buf.write(meta_info)

        buf.seek(0)
        fp.write(buf.read())
        fp.seek(seg_offset+4096)

        #update rddm segments data
        off = 4096

        for seg in self.segments:
                data = self.buf[off:off + seg.len]

                if (seg.type == ATH11K_FW_QDSS_DATA):
                        fq.write(data)
                        off = off + seg.len
                        continue

                fp.write(data)
                off = off + seg.len

        fp.close()
        if (self.is_qdss):
            fq.close()

    def __repr__(self):
        return self.to_string(raw=True)


    def to_string(self):
        s = ''

        s = s + 'Length: %d\n' % (self.len)
        s = s + 'Version: %d\n' % (self.version)
        s = s + 'UUID: %s\n' % (self.guid)
        s = s + 'Timestamp: %s.%s\n' % (self.tv_sec, self.tv_nsec)
        s = s + 'Date: %s UTC\n' % (time.asctime(time.gmtime(self.tv_sec)))
        s = s + 'qrtr_id: %s\n' % (self.qrtr_id)
        s = s + 'Chip_id: 0x%08x\n' % (self.chip_id)
        s = s + 'Bus_id: %d\n' % (self.bus_id)
        s = s + 'Num_seg: %d\n' % (self.num_seg)

        s = s.strip('\n')
        return s


def main():
    parser = argparse.ArgumentParser(description='Show and extract information from ath11k coredump file.')

    parser.add_argument('filename', metavar='FILE', help='the coredump file')

    # actions
    action_group = parser.add_mutually_exclusive_group(required=True)
    action_group.add_argument('-i', '--info', action='store_true',
                              help='print all hdr data')
    action_group.add_argument('-x', '--extract', action='store_true',
                              help='generate elf coredump file from input crash dump')

    args = parser.parse_args()

    if args.info:
        dump = Ath11kFwCrashDump()
        dump.parse(args.filename)
        print('ATH11K DUMP FILE DATA: \n')
        print(dump.to_string())
        print('\n')
        metaInfo = dump.get_meta_info()
        metaInfo.print_meta_info()
    elif args.extract:
        dump = Ath11kFwCrashDump()
        dump.parse(args.filename)
        dump.generate_bin()     #creates paging, remote, fwbin, dyn_paging & qdss, m3 output files
        dump.generate_coredump()
    else:
        # should not happen
        print('Error: no action defined')
        sys.exit(1)


main()
