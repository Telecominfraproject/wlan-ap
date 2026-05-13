
/*
 * Copyright 2022 Morse Micro
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "morsectrl.h"
#include "transport/transport.h"
#include "utilities.h"

#define TRANSRAW_OK_TYPE(x) (((x) == TRANSRAW_READ) || \
                             ((x) == TRANSRAW_READ_TO_FILE) || \
                             ((x) == TRANSRAW_WRITE) || \
                             ((x) == TRANSRAW_WRITE_FROM_FILE))


#define CHIP_ID_ADDR                (0x10054d20)

#define IMEM_BANK0_ADDR             (0x00100000)
#define IMEM_BANK1_ADDR             (0x00110000)
#define IMEM_BANK2_ADDR             (0x00120000)
#define IMEM_BANK3_ADDR             (0x00130000)
#define IMEM_BANK4_ADDR             (0x00140000)
#define IMEM_BANK5_ADDR             (0x00150000)
#define IMEM_BANK6_ADDR             (0x00158000)

#define DMEM_BANK0_ADDR             (0x80100000)
#define DMEM_BANK1_ADDR             (0x80200000)
#define DMEM_BANK2_ADDR             (0x80300000)
#define DMEM_BANK3_ADDR             (0x80400000)

#define REG_TEST_VALUE_BASE         (0x12340000)
#define BYTE_TEST_VALUE_BASE        (0x23450000)
#define BLOCK_TEST_VALUE_BASE       (0x34560000)
#define BLOCK1_5_TEST_VALUE_BASE    (0x45670000)
#define BLOCK2_TEST_VALUE_BASE      (0x56780000)
#define BLOCK2_5_TEST_VALUE_BASE    (0x67890000)
#define BOUND_TEST_VALUE_BASE       (0x789A0000)

#define BYTE_TEST_SIZE              (sizeof(uint32_t) * 16)
#define BLOCK_TEST_SIZE             (512)
#define BLOCK1_5_TEST_SIZE          (512 + 256)
#define BLOCK2_TEST_SIZE            (2 * 512)
#define BLOCK2_5_TEST_SIZE          ((2 * 512) + 256)
#define BOUND_TEST_SIZE             (UINT16_MAX + 1 + BLOCK1_5_TEST_SIZE)

typedef enum
{
    TRANSRAW_UNKNOWN,
    TRANSRAW_UNKNOWN_FILE,
    TRANSRAW_WRITE,
    TRANSRAW_WRITE_FROM_FILE,
    TRANSRAW_READ,
    TRANSRAW_READ_TO_FILE,
    TRANSRAW_TEST,
} transraw_type_t;

static void usage(struct morsectrl *mors)
{
    mctrl_print("\ttransraw [-a <address> [-w <value>] [-w -f <path to binary>] [-r <read_value>]] [-t]\n"); /* NOLINT */
    mctrl_print("\t\t\t\tWrites or reads raw memory in the chip via transport\n");
    mctrl_print(
        "\t\t\t\tThis command only supports transports that interface directly to the chip\n");
}

static bool transport_buff_is_equal(struct morsectrl_transport_buff *a,
                                    struct morsectrl_transport_buff *b)
{
    if (!a || !b)
        return false;

    if (a->data_len != b->data_len)
        return false;

    return !memcmp(a->data, b->data, a->data_len);
}

/**
 * @brief Runs a memory tests to exercise a piece of memory.
 *
 * @param transport     The transport structure.
 * @param base_addr     The address to start writing/reading to/from.
 * @param size          The size of the memory to exercise.
 * @param base_value    A starting data value. Unique values should be used when exercising multiple
 *                      memory spaces/or sizes to prevent creating false positives.
 * @return              0 on success otherwise relevant error.
 */
static int transraw_memtest(struct morsectrl_transport *transport,
                            uint32_t base_addr,
                            uint32_t size,
                            uint32_t base_value)
{
    int ret;
    uint32_t ii;

    struct morsectrl_transport_buff *write_buff;
    struct morsectrl_transport_buff *read_buff;

    write_buff = morsectrl_transport_raw_write_alloc(transport, size);
    read_buff = morsectrl_transport_raw_read_alloc(transport, size);

    if (!write_buff || !read_buff)
    {
        ret = -1;
        goto exit;
    }

    for (ii = 0; (ii + 3) < write_buff->data_len; ii += 4)
    {
        write_buff->data[ii] = ii & 0xFF;
        write_buff->data[ii + 1] = (ii >> 8) & 0xFF;
        write_buff->data[ii + 2] = (base_value >> 16) & 0xFF;
        write_buff->data[ii + 3] = (base_value >> 24) & 0xFF;
    }

    ret = morsectrl_transport_mem_write(transport, write_buff, base_addr);
    if (ret)
    {
        mctrl_err("Mem write failed\n");
        goto exit;
    }

    ret = morsectrl_transport_mem_read(transport, read_buff, base_addr);
    if (ret)
    {
        mctrl_err("Mem read failed\n");
        goto exit;
    }

    if (!transport_buff_is_equal(write_buff, read_buff))
    {
        ret = -1;
        mctrl_err("Mem write not equal to read\n");

        for (ii = 0; ii < write_buff->data_len; ii++)
        {
            if (write_buff->data[ii] != read_buff->data[ii])
                mctrl_err("Difference starts at octet 0x%08x: 0x%02x != 0x%02x\n",
                       ii, write_buff->data[ii], read_buff->data[ii]);
        }
    }

exit:
    morsectrl_transport_buff_free(write_buff);
    morsectrl_transport_buff_free(read_buff);

    return ret;
}


 /**
  * @brief Runs a memory tests to exercise different sizes and types of writes/reads.
  *
  * @note The sizes of writes/reads are targeted to SDIO but maybe useful for other transports.
  *
  * @param transport    The transport structure.
  * @return             0 on success, otherwise relevant error.
  */
static int transraw_test(struct morsectrl_transport *transport)
{
    int ret;
    uint32_t reg_write_val = REG_TEST_VALUE_BASE;
    uint32_t reg_read_val;
    int ii;

    mctrl_print("Beginning transport test\n");

    /* Read Chip ID. */
    ret = morsectrl_transport_reg_read(transport, CHIP_ID_ADDR, &reg_read_val);
    if (ret)
        goto exit;

    mctrl_print("\nChip ID: 0x%04x\n", reg_read_val);

    /* Write some single registers first. */
    mctrl_print("\nWrite and read registers:\n");
    for (ii = IMEM_BANK0_ADDR; ii <= IMEM_BANK5_ADDR; ii += IMEM_BANK1_ADDR - IMEM_BANK0_ADDR)
    {
        ret = morsectrl_transport_reg_write(transport, ii, reg_write_val);
        if (ret)
            goto exit;

        reg_write_val++;
    }

    /* Read the registers to make sure the values are as expected. */
    reg_write_val = REG_TEST_VALUE_BASE;
    for (ii = IMEM_BANK0_ADDR; ii <= IMEM_BANK5_ADDR; ii += IMEM_BANK1_ADDR - IMEM_BANK0_ADDR)
    {
        bool passed;
        ret = morsectrl_transport_reg_read(transport, ii, &reg_read_val);
        if (ret)
            goto exit;

        passed = (reg_write_val == reg_read_val);
        mctrl_print("0x%08x: 0x%08x %c= 0x%08x - %s\n",
               ii + 1,
               reg_write_val, passed ? '=' : '!',
               reg_read_val, passed ? "Pass" : "Fail");

        if (!passed)
        {
            ret = -1;
            goto exit;
        }

        reg_write_val++;
    }

    mctrl_print("\nWrite and read memory blocks:\n");
    mctrl_print("Write and read bytes (IMEM Bank 0)                              - ");
    /* Write some memory. */
    ret = transraw_memtest(transport, IMEM_BANK0_ADDR,
                           BYTE_TEST_SIZE, BYTE_TEST_VALUE_BASE);
    if (ret)
    {
        mctrl_err("Fail\n");
        goto exit;
    }
    mctrl_print("Pass\n");


    mctrl_print("Write and read bytes (IMEM Bank 1)                              - ");
    /* Write some memory to a different bank. */
    ret = transraw_memtest(transport, IMEM_BANK1_ADDR,
                           BYTE_TEST_SIZE, BYTE_TEST_VALUE_BASE);
    if (ret)
    {
        mctrl_err("Fail\n");
        goto exit;
    }
    mctrl_print("Pass\n");

    mctrl_print("Write and read single block                                     - ");
    ret = transraw_memtest(transport, IMEM_BANK0_ADDR,
                           BLOCK_TEST_SIZE, BLOCK_TEST_VALUE_BASE);
    if (ret)
    {
        mctrl_err("Fail\n");
        goto exit;
    }
    mctrl_print("Pass\n");

    mctrl_print("Write and read single block and then bytes                      - ");
    ret = transraw_memtest(transport, IMEM_BANK0_ADDR,
                           BLOCK1_5_TEST_SIZE, BLOCK2_5_TEST_VALUE_BASE);
    if (ret)
    {
        mctrl_err("Fail\n");
        goto exit;
    }
    mctrl_print("Pass\n");

    mctrl_print("Write and read multi blocks                                     - ");
    ret = transraw_memtest(transport, IMEM_BANK0_ADDR,
                           BLOCK2_TEST_SIZE, BLOCK2_TEST_VALUE_BASE);
    if (ret)
    {
        mctrl_err("Fail\n");
        goto exit;
    }
    mctrl_print("Pass\n");

    mctrl_print("Write and read multi blocks and then bytes                      - ");
    ret = transraw_memtest(transport, IMEM_BANK0_ADDR,
                           BLOCK2_5_TEST_SIZE, BLOCK2_5_TEST_VALUE_BASE);
    if (ret)
    {
        mctrl_err("Fail\n");
        goto exit;
    }
    mctrl_print("Pass\n");

    mctrl_print("Write and read multi blocks and then bytes across 64kB boundary - ");
    ret = transraw_memtest(transport, IMEM_BANK0_ADDR,
                           BOUND_TEST_SIZE, BOUND_TEST_VALUE_BASE);
    if (ret)
    {
        mctrl_err("Fail\n");
        goto exit;
    }
    mctrl_print("Pass\n");

exit:
    return ret;
}

int transraw(struct morsectrl *mors, int argc, char *argv[])
{
    char *path;
    int option;
    uint32_t addr = 0;
    transraw_type_t type = TRANSRAW_UNKNOWN;
    FILE *file;
    size_t file_size;
    uint32_t read_val;
    uint32_t write_val;
    struct morsectrl_transport *transport = mors->transport;
    struct morsectrl_transport_buff *buff = NULL;
    int ret = 0;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    while ((option = getopt(argc, argv, "a:f:w:rt")) != -1)
    {
        switch (option)
        {
        case 'a':
            if (str_to_uint32(optarg, &addr))
            {
                mctrl_err("Invalid address\n");
                return -1;
            }
            break;

        case 'w':
            str_to_uint32(optarg, &write_val);

            switch (type)
            {
            case TRANSRAW_UNKNOWN:
                type = TRANSRAW_WRITE;
                break;

            case TRANSRAW_UNKNOWN_FILE:
                type = TRANSRAW_WRITE_FROM_FILE;
                break;

            default:
                usage(mors);
                return -1;
            }
            break;

        case 'r':
            switch (type)
            {
            case TRANSRAW_UNKNOWN:
                type = TRANSRAW_READ;
                break;

            case TRANSRAW_UNKNOWN_FILE:
                type = TRANSRAW_READ_TO_FILE;
                break;

            default:
                usage(mors);
                return -1;
            }
            break;

        case 'f':
            path = optarg;
            switch (type)
            {
            case TRANSRAW_UNKNOWN:
                type = TRANSRAW_UNKNOWN_FILE;
                break;

            case TRANSRAW_READ:
                type = TRANSRAW_READ_TO_FILE;
                break;

            case TRANSRAW_WRITE:
                type = TRANSRAW_WRITE_FROM_FILE;
                break;

            default:
                usage(mors);
                return -1;
            }
            break;

        case 't':
            type = TRANSRAW_TEST;
            break;

        default:
            usage(mors);
            return -1;
        }
    }

    switch (type)
    {
    case TRANSRAW_READ_TO_FILE:
        file = fopen(path, "wb");
        if (!file)
        {
            ret = -1;
            goto fail;
        }
        buff = morsectrl_transport_raw_read_alloc(transport, read_val);
        ret = morsectrl_transport_mem_read(transport, buff, addr);
        if (fwrite(buff->data, sizeof(*buff->data), read_val, file) != read_val)
            ret = -1;
        fclose(file);
        break;

    case TRANSRAW_WRITE_FROM_FILE:
        file = fopen(path, "rb");
        if (!file)
        {
            ret = -1;
            goto fail;
        }
        file_size = get_file_size(file);
        buff = morsectrl_transport_raw_write_alloc(transport, file_size);
        if (!buff)
        {
            ret = -1;
            fclose(file);
            goto fail;
        }
        load_file(file, &buff->data);
        ret = morsectrl_transport_mem_write(transport, buff, addr);
        fclose(file);
        break;

    case TRANSRAW_READ:
        morsectrl_transport_reg_read(transport, addr, &read_val);
        mctrl_print("0x%08x\n", read_val);
        break;

    case TRANSRAW_WRITE:
        morsectrl_transport_reg_write(transport, addr, write_val);
        mctrl_print("0x%08x\n", write_val);
        break;

    case TRANSRAW_TEST:
        return transraw_test(mors->transport);

    case TRANSRAW_UNKNOWN:
    case TRANSRAW_UNKNOWN_FILE:
    default:
        usage(mors);
        return -1;
    }

    morsectrl_transport_buff_free(buff);

fail:
    if (ret)
        mctrl_print("Transport RAW read/write failure\n");

    return ret;
}

MM_CLI_HANDLER(transraw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
