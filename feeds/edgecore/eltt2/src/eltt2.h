#ifndef _ELTT2_H_
#define _ELTT2_H_
/**
  * @brief		Infineon Embedded Linux TPM Toolbox 2 (ELTT2) for TPM 2.0
  * @details		eltt2.h implements all TPM byte commands and the prototype declarations for eltt2.c.
  * @file		eltt2.h
  * @date		2014/06/26
  * @copyright	Copyright (c) 2014 - 2017 Infineon Technologies AG ( www.infineon.com ).\n
  * All rights reserved.\n
  * \n
  * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following
  * conditions are met:\n
  * \n
  * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
  * disclaimer.\n
  * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
  * disclaimer in the documentation and/or other materials provided with the distribution.\n
  * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
  * derived from this software without specific prior written permission.\n
  * \n
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */

// this is the main page for doxygen documentation.
/** @mainpage	Infineon Embedded Linux TPM Toolbox 2 (ELTT2) for TPM 2.0 Documentation
  *
  * @section	Welcome
  * Welcome to Infineon TPM 2.0 Software-Tool "Embedded Linux TPM Toolbox 2 (ELTT2)".\n
  * \n
  * @section	Introduction
  * ELTT2 is a single file-executable program
  * intended for test, diagnosis and basic state changes of the Infineon
  * Technologies TPM 2.0.\n
  * \n
  * @section	Copyright
  * Copyright (c) 2014 - 2017 Infineon Technologies AG ( www.infineon.com ).\n
  * All rights reserved.\n
  * \n
  * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following
  * conditions are met:\n
  * \n
  * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
  * disclaimer.\n
  * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
  * disclaimer in the documentation and/or other materials provided with the distribution.\n
  * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
  * derived from this software without specific prior written permission.\n
  * \n
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>

//-------------"Defines"-------------
#define TPM_RESP_MAX_SIZE		4096	///< This is the maximum possible TPM response size in bytes.
#define TPM_REQ_MAX_SIZE		1024	///< This is the maximum possible TPM request size in bytes. TBD: Find out correct value.
#define ERR_COMMUNICATION		-1	///< Return error check for read and write to the TPM.
#define ERR_BAD_CMD			-2	///< Error code for a bad command line argument or option.
#define TPM_SHA1_DIGEST_SIZE		20	///< For all SHA-1 operations the digest's size is always 20 bytes.
#define TPM_SHA256_DIGEST_SIZE		32	///< For all SHA-256 operations the digest's size is always 32 bytes.
#define TPM_SHA384_DIGEST_SIZE		48	///< For all SHA-384 operations the digest's size is always 48 bytes.
#define TPM_CMD_HEADER_SIZE		10	///< The size of a standard TPM command header is 10 bytes.
#define TPM_CMD_SIZE_OFFSET		2	///< The offset of a TPM command's size value is 2 bytes.
#define HEX_BYTE_STRING_LENGTH		2	///< A byte can be represented by two hexadecimal characters.
#ifndef INT_MAX
#define INT_MAX 0x7FFFFFF			///< The maximum value of a signed 32-bit integer.
#endif
// TPM Return codes
#define TPM_RC_SUCCESS			0x00000000	///< The response error code for TPM_SUCCESS.
#define TPM_RC_BAD_TAG			0x0000001E	///< The response error code for TPM_RC_BAD_TAG.
#define TPM_RC_SIZE			0x00000095	///< The response error code for TPM_RC_SIZE.
#define TPM_RC_INITIALIZE		0x00000100	///< The response error code for TPM_RC_INITIALIZE.
#define TPM_RC_FAILURE			0x00000101	///< The response error code for TPM_RC_FAILURE.
#define TPM_RC_LOCALITY			0x00000907	///< The response error code for TPM_RC_LOCALITY.
#define FU_FIRMWARE_VALID_FLAG		4		///< If this flag is set, the firmware is valid.
#define FU_OWNER_FLAG			1		///< If this flag is set, the owner is set.
// print_response_buf options
#define PRINT_RESPONSE_CLEAR			1	///< Prints response unformatted.
#define PRINT_RESPONSE_HEADERBLOCKS		2	///< Prints response in commented blocks.
#define PRINT_RESPONSE_HEX_BLOCK		3	///< Prints response in rows of 16 bytes and shows the line number.
#define PRINT_RESPONSE_HASH			4	///< Prints response of Hash
#define PRINT_RESPONSE_WITHOUT_HEADER		12	///< Prints the response buffer from byte 12.
#define PRINT_RESPONSE_HASH_WITHOUT_HEADER	16	///< Prints the response buffer from byte 16.
#define PRINT_RESPONSE_WITH_HEADER		0	///< Prints the response buffer from byte 0.
#define PRINT_RESPONSE_PCR_WITHOUT_HEADER	30	///< Prints the pcr buffer from pcr_read.
// time conversion
#define YEAR_SECONDS			31536000	///< Number of seconds in one year
#define DAY_SECONDS			86400		///< Number of seconds in one day
#define HOUR_SECONDS			3600		///< Number of seconds in one hour
#define MINUTE_SECONDS			60		///< Number of seconds in one minute
#define MILISECOND_TO_SECOND		1000		///< Convertion from miliseconds to seconds
// hash
#define STD_CC_HASH_SIZE		18		///< Hash command size
// TPM_PT constants
#define PT_FIXED_SELECTOR		1		///< Fixed GetCapability Flags
#define PT_VAR_SELECTOR			2		///< Variable GetCapability Flags

//-------------"Macros"-------------
// Null pointer check
#define NULL_POINTER_CHECK(x) if (NULL == x) { ret_val = EINVAL; fprintf(stderr, "Error: Invalid argument.\n"); break; }	///< Argument NULL check.
#define MALLOC_ERROR_CHECK(x) if (NULL == x) { ret_val = errno; fprintf(stderr, "Error (re)allocating memory.\n"); break; }	///< Malloc error check.
#define MEMSET_FREE(x, y) if (NULL != x) { memset(x, 0, y); free(x); x = NULL; } ///< Sets memory to 0, frees memory and sets pointer to NULL.
// Return value check
#define RET_VAL_CHECK(x) if (EXIT_SUCCESS != x) { break; } ///< Return value check
// Command line option parser for hash algorithm
#define HASH_ALG_PARSER(o, c) \
	do { \
		if (o == option) \
		{ \
			if (c == argc) \
			{ \
				hash_algo = ALG_SHA1; \
			} \
			else \
			{ \
				if (0 == strcasecmp(optarg, "sha1")) \
				{ \
					hash_algo = ALG_SHA1; \
				} \
				else if (0 == strcasecmp(optarg, "sha256")) \
				{ \
					hash_algo = ALG_SHA256; \
				} \
				else if (0 == strcasecmp(optarg, "sha384")) \
				{ \
					hash_algo = ALG_SHA384; \
				} \
				else \
				{ \
					ret_val = ERR_BAD_CMD; \
					fprintf(stderr, "Unknown option. Use '-h' for more information.\n"); \
					break; \
				} \
				if (argc > optind) \
				{ \
					optarg = argv[optind++]; \
				} \
			} \
		} \
		else \
		{ \
			hash_algo = ALG_SHA256; \
		} \
	} while (0)

//--------------"Enums"--------------
// Hash algorithms
typedef enum hash_algo_enum
{
	ALG_NULL,
	ALG_SHA1,
	ALG_SHA256,
	ALG_SHA384,
} hash_algo_enum;

//-------------"Methods"-------------
/**
  * @brief	Convert (max.) 8 byte buffer to an unsigned 64-bit integer.
  * @param	[in]	*input_buffer		Input buffer. Make sure that its size is at least as high as offset + length.
  * @param	[in]	offset			Start byte for conversion.
  * @param	[in]	length			Amount of bytes to be converted.
  * @param	[out]	*output_value		Return the converted unsigned 64-bit integer.
  * @param	[in]	input_buffer_size	Size of input_buffer in bytes.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer or length is greater than 8.
  * @retval	EXIT_SUCCESS			In case of success.
  * @date	2014/06/26
  */
static int buf_to_uint64(uint8_t *input_buffer, uint32_t offset, uint32_t length, uint64_t *output_value, uint32_t input_buffer_size);

/**
  * @brief	Convert a hexadecimal string representation of bytes like "0A1F" and
		returns an array containing the actual byte values as an array (e.g. { 0x0A, 0x1F }).
  * @param	[in]	*byte_string		Incoming bytes as string.
  * @param	[out]	*byte_values		Byte array representation of given input string.
  * 						Must be allocated by caller with the length given in byte_values_size.
  * @param	[in]	byte_values_size	Size of byte_values array.
  * @return	One of the listed return codes.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	value of errno			In case parsing error.
  * @date	2014/06/26
  */
static int hexstr_to_bytearray(char *byte_string, uint8_t *byte_values, size_t byte_values_size);

/**
  * @brief	Convert a number to a byte buffer.
  * @param	[in]	input			User input.
  * @param	[in]	input_size		Size of input data type in bytes.
  * @param	[out]	*output_byte		Return buffer for the converted integer.
						Must be allocated by the caller with at least a size of 'input_size'.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @date	2014/06/26
  */
static int int_to_bytearray(uint64_t input, uint32_t input_size, uint8_t *output_byte);

/**
  * @brief	Create the PCR_Extend command.
  * @param	[in]	*pcr_index_str		User input string for PCR index.
  * @param	[in]	*pcr_digest_str		User input string of value to extend the selected PCR with.
  * @param	[out]	*pcr_cmd_buf		Return buffer for the complete command. Must be allocated by caller.
  * @param	[in]	*pcr_cmd_buf_size	Size of memory allocated at pcr_cmd_buf in bytes.
  * @param	[in]	hash_algo		Set to ALG_SHA1 for extending with SHA-1,
						ALG_SHA256 for SHA-256, and ALG_SHA384 for SHA-384.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer or an invalid option.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	ERR_BAD_CMD			In case of bad user input.
  * @retval	hexstr_to_bytearray		All error codes from hexstr_to_bytearray.
  * @date	2014/06/26
  */
static int pcr_extend(char *pcr_index_str, char *pcr_digest_str, uint8_t *pcr_cmd_buf, size_t pcr_cmd_buf_size, hash_algo_enum hash_algo);

/**
  * @brief	Create the PCR_Allocate command.
  * @param	[out]	*pcr_cmd_buf		Return buffer for the complete command.
  * @param	[in]	hash_algo		Set to ALG_SHA1 to allocate SHA-1,
						ALG_SHA256 for SHA-256, and ALG_SHA384 for SHA-384.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer or an invalid option.
  * @retval	EXIT_SUCCESS			In case of success.
  * @date	2022/05/09
  */
static int pcr_allocate(uint8_t *pcr_cmd_buf, hash_algo_enum hash_algo);

/**
  * @brief	Create the PCR_Read command.
  * @param	[in]	*pcr_index_str		User input string for PCR index.
  * @param	[out]	*pcr_cmd_buf		Return buffer for the complete command.
  * @param	[in]	hash_algo		Set to ALG_SHA1 for reading with SHA-1,
						ALG_SHA256 for SHA-256, and ALG_SHA384 for SHA-384.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer or an invalid option.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	ERR_BAD_CMD			In case of bad user input.
  * @retval	hexstr_to_bytearray		All error codes from hexstr_to_bytearray.
  * @date	2014/06/26
  */
static int pcr_read(char *pcr_index_str, uint8_t *pcr_cmd_buf, hash_algo_enum hash_algo);

/**
  * @brief	Create the PCR_Reset command.
  * @param	[in]	*pcr_index_str		User input string for PCR index.
  * @param	[out]	*pcr_cmd_buf		Return buffer for the complete command.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	ERR_BAD_CMD			In case of bad user input.
  * @retval	hexstr_to_bytearray		All error codes from hexstr_to_bytearray.
  * @date	2014/06/26
  */
static int pcr_reset(char *pcr_index_str, uint8_t *pcr_cmd_buf);

/**
  * @brief	Print the command line usage and switches.
  * @date	2014/06/26
  */
static void print_help();

/**
  * @brief	Print the response buffer in different formats.
  * @param	[in]	*response_buf		TPM response.
  * @param	[in]	resp_size		TPM response size.
  * @param	[in]	offset			Starting point for printing buffer.
  * @param	[in]	format			Select the output format.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer or an unknown output format has been transfered.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	buf_to_uint64			All error codes from buf_to_uint64.
  * @date	2014/06/26
  */
static int print_response_buf(uint8_t *response_buf, size_t resp_size, uint32_t offset, int format);

/**
  * @brief	Print a TPM response.
  * @param	[in]	*response_buf		TPM response.
  * @param	[in]	resp_size		TPM response size.
  * @param	[in]	option			Defines appearance of output. Can have the following values:\n
						- PRINT_RESPONSE_CLEAR
						- PRINT_RESPONSE_HEADERBLOCKS
						- PRINT_RESPONSE_HEX_BLOCK
						- PRINT_RESPONSE_WITHOUT_HEADER
						- PRINT_RESPONSE_WITH_HEADER
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	print_response_buf		All error codes from print_response_buf.
  * @retval	print_clock_info		All error codes from print_clock_info.
  * @retval	print_capability_flags		All error codes from print_capability_flags.
  * @date	2014/06/26
  */
static int response_print(uint8_t *response_buf, size_t resp_size, int option);

/**
  * @brief	Check a TPM response for errors.
  * @param	[in]	*response_buf		TPM response. Must have at least a size of TPM_CMD_HEADER_SIZE bytes.
  * @return	Returns the TPM return code extracted from the given TPM response or one of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	buf_to_uint64			All error codes from buf_to_uint64.
  * @retval	EXIT_SUCCESS			In case of success.
  * @date	2014/06/26
  */
static int return_error_handling(uint8_t *response_buf);

/**
  * @brief	Transmit TPM command to /dev/tpm0 and get the response.
  * @param	[in]	*buf			TPM request.
  * @param	[in]	length			TPM request length.
  * @param	[out]	*response		TPM response.
  * @param	[out]	*resp_length		TPM response length.
  * @return	One of the listed return codes or the error code stored in the global errno system variable.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @date	2014/06/26
  */
static int tpmtool_transmit(const uint8_t *buf, ssize_t length, uint8_t *response, ssize_t *resp_length);

/**
  * @brief	Print the capability flags.
  * @param	[in]	*response_buf		TPM response.
  * @param	[in]	cap_selector		Type of capabilities to print.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	buf_to_uint64			All error codes from buf_to_uint64.
  * @date	2014/06/26
  */
static int print_capability_flags(uint8_t *response_buf, uint8_t cap_selector);

/**
  * @brief	Print the clock info.
  * @param	[in]	*response_buf		TPM response.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	buf_to_uint64			All error codes from buf_to_uint64.
  * @date	2014/06/26
  */
static int print_clock_info(uint8_t *response_buf);

/**
  * @brief	Create the get_random command.
  * @param	[in]	*data_length_string	User input string for random data length.
  * @param	[out]	*response_buf		Return buffer for the complete command.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	ERR_BAD_CMD			In case of bad user input.
  * @retval	hexstr_to_bytearray		All error codes from hexstr_to_bytearray.
  * @date	2014/06/26
  */
static int get_random(char *data_length_string, uint8_t *response_buf);

/**
  * @brief	Create the simple hash command.
  * @param	[in]	*data_string		User input string of data to be hashed.
  * @param	[in]	hash_algo		Set to ALG_SHA1 for hashing with SHA-1,
						ALG_SHA256 for SHA-256, and ALG_SHA384 for SHA-384.
  * @param	[out]	*hash_cmd_buf		Return buffer for the complete command.
  * @param	[in]	hash_cmd_buf_size	Return buffer size.
  * @return	One of the listed return codes.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	hexstr_to_bytearray		All error codes from hexstr_to_bytearray.
  * @retval	int_to_bytearray		All error codes from int_to_bytearray.
  * @date	2014/06/26
  */
static int create_hash(char *data_string, hash_algo_enum hash_algo, uint8_t *hash_cmd_buf, uint32_t hash_cmd_buf_size);

/**
  * @brief	Create and transmit a sequence of TPM commands for hashing larger amounts of data.
  * @param	[in]	*data_string		User input string of data to be hashed.
  * @param	[in]	hash_algo		Set to ALG_SHA1 for hashing with SHA-1,
						ALG_SHA256 for SHA-256, and ALG_SHA384 for SHA-384.
  * @param	[out]	*tpm_response_buf	TPM response.
  * @param	[out]	*tpm_response_buf_size	Size of tpm_response_buf.
  * @return	One of the listed return codes or the error code stored in the global errno system variable.
  * @retval	EINVAL				In case of a NULL pointer.
  * @retval	EXIT_SUCCESS			In case of success.
  * @retval	value of errno			In case of memory allocation error.
  * @retval	buf_to_uint64			All error codes from buf_to_uint64.
  * @retval	hexstr_to_bytearray		All error codes from hexstr_to_bytearray.
  * @retval	int_to_bytearray		All error codes from int_to_bytearray.
  * @retval	tpmtool_transmit		All error codes from tpmtool_transmit.
  * @retval	print_response_buf		All error codes from print_response_buf
  * @date	2014/06/26
  */
static int create_hash_sequence(char *data_string, hash_algo_enum hash_algo, uint8_t *tpm_response_buf, ssize_t *tpm_response_buf_size);

//-------------"command bytes"-------------
static const uint8_t tpm2_startup_clear[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0C,		// commandSize
	0x00, 0x00, 0x01, 0x44,		// TPM_CC_Startup
	0x00, 0x00			// TPM_SU_CLEAR
};

static const uint8_t tpm2_startup_state[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0C,		// commandSize
	0x00, 0x00, 0x01, 0x44,		// TPM_CC_Startup
	0x00, 0x01			// TPM_SU_STATE
};

static const uint8_t tpm_cc_shutdown_clear[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0C,		// commandSize
	0x00, 0x00, 0x01, 0x45,		// TPM_CC_Shutdown
	0x00, 0x00			// TPM_SU_CLEAR
};

static const uint8_t tpm_cc_shutdown_state[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0C,		// commandSize
	0x00, 0x00, 0x01, 0x45,		// TPM_CC_Shutdown
	0x00, 0x01			// TPM_SU_STATE
};

static const uint8_t tpm2_self_test[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0B,		// commandSize
	0x00, 0x00, 0x01, 0x43,		// TPM_CC_SelfTest
	0x00				// fullTest=No
};

static const uint8_t tpm2_self_test_full[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0B,		// commandSize
	0x00, 0x00, 0x01, 0x43,		// TPM_CC_SelfTest
	0x01				// fullTest=Yes
};

static const uint8_t tpm_cc_get_test_result[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0A,		// commandSize
	0x00, 0x00, 0x01, 0x7C		// TPM_CC_GetTestResult
};

static const uint8_t tpm2_self_test_incremental[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x2A,		// commandSize
	0x00, 0x00, 0x01, 0x42,		// TPM_CC_IncrementalSelfTest
	0x00, 0x00, 0x00, 0x0E,		// Count of Algorithm
	0x00, 0x01, 0x00, 0x04,		// Algorithm two per line
	0x00, 0x05, 0x00, 0x06,
	0x00, 0x08, 0x00, 0x0A,
	0x00, 0x0B, 0x00, 0x14,
	0x00, 0x15, 0x00, 0x16,
	0x00, 0x17, 0x00, 0x22,
	0x00, 0x25, 0x00, 0x43
};

static const uint8_t tpm2_getrandom[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0C,		// commandSize
	0x00, 0x00, 0x01, 0x7B,		// TPM_CC_GetRandom
	0x00, 0x00			// bytesRequested (will be set later)
};

static const uint8_t tpm_cc_readclock[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0A,		// commandSize
	0x00, 0x00, 0x01, 0x81		// TPM_CC_ReadClock
};

static const uint8_t tpm2_getcapability_fixed[] ={
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x16,		// commandSize
	0x00, 0x00, 0x01, 0x7A,		// TPM_CC_GetCapability
	0x00, 0x00, 0x00, 0x06,		// TPM_CAP_TPM_PROPERTIES (Property Type: TPM_PT)
	0x00, 0x00, 0x01, 0x00,		// Property: TPM_PT_FAMILY_INDICATOR: PT_GROUP * 1 + 0
	0x00, 0x00, 0x00, 0x66		// PropertyCount 102 (from 100 - 201)
};

static const uint8_t tpm2_getcapability_var[] ={
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x16,		// commandSize
	0x00, 0x00, 0x01, 0x7A,		// TPM_CC_GetCapability
	0x00, 0x00, 0x00, 0x06,		// TPM_CAP_TPM_PROPERTIES (Property Type: TPM_PT)
	0x00, 0x00, 0x02, 0x00,		// Property: TPM_PT_FAMILY_INDICATOR: PT_GROUP * 2 + 0
	0x00, 0x00, 0x00, 0x02		// PropertyCount 02 (from 200 - 201)
};

// Hash
static const uint8_t tpm2_hash[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0e,		// commandSize
	0x00, 0x00, 0x01, 0x7D,		// TPM_CC_Hash
	0x00, 0x00,			// size (will be set later)
					// buffer (will be added later)
	0x00, 0x00,			// hashAlg (will be added later)
	0x00, 0x00, 0x00, 0x00		// hierarchy of the ticket (TPM_RH_NULL; will be added later)
};

// HashSequence
static uint8_t tpm2_hash_sequence_start[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x0e,		// commandSize
	0x00, 0x00, 0x01, 0x86,		// TPM_CC_HashSequenceStart
	0x00, 0x00,			// authSize (NULL Password)
					// null (indicate a NULL Password)
	0x00, 0x00			// hashAlg (will be set later)
};

static uint8_t tpm2_sequence_update[] = {
	0x80, 0x02,			// TPM_ST_SESSIONS
	0x00, 0x00, 0x00, 0x00,		// commandSize (will be set later)
	0x00, 0x00, 0x01, 0x5c,		// TPM_CC_SequenceUpdate
	0x00, 0x00, 0x00, 0x00,		// sequenceHandle (will be set later)
	0x00, 0x00,			// authSize (NULL Password)
					// null (indicate a NULL Password)
	0x00, 0x09,			// authSize (password authorization session)
	0x40, 0x00, 0x00, 0x09,		// TPM_RS_PW (indicate a password authorization session)
	0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00			// size (will be set later)
					// buffer (will be added later)
};

static uint8_t tpm2_sequence_complete[] = {
	0x80, 0x02,			// TPM_ST_SESSIONS
	0x00, 0x00, 0x00, 0x21,		// commandSize
	0x00, 0x00, 0x01, 0x3e,		// TPM_CC_SequenceComplete
	0x00, 0x00, 0x00, 0x00,		// sequenceHandle (will be set later)
	0x00, 0x00,			// authSize (NULL Password)
					// null (indicate a NULL Password)
	0x00, 0x09,			// authSize (password authorization session)
	0x40, 0x00, 0x00, 0x09,		// TPM_RS_PW (indicate a password authorization session)
	0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00,			// size (NULL buffer)
					// null (indicate an empty buffer buffer)
	0x40, 0x00, 0x00, 0x07		// hierarchy of the ticket (TPM_RH_NULL)
};

static const uint8_t sha1_alg[] = {
	0x00, 0x04			// command for sha1 alg
};

static const uint8_t sha256_alg[] = {
	0x00, 0x0B			// command for sha256 alg
};

static const uint8_t sha384_alg[] = {
	0x00, 0x0C			// command for sha384 alg
};

static const uint8_t tpm_cc_hash_hierarchy[] = {
	0x40, 0x00, 0x00, 0x07		// hierarchy of the ticket (TPM_RH_NULL)
};

//PCR_Command
static const uint8_t tpm2_pcr_allocate[] = {
	0x80, 0x02,			// TPM_ST_SESSIONS
	0x00, 0x00, 0x00, 0x31,		// commandSize
	0x00, 0x00, 0x01, 0x2B,		// TPM_CC_PCR_Allocate
	0x40, 0x00, 0x00, 0x0C,		// TPM_RH_PLATFORM
	0x00, 0x00,			// authSize (NULL Password)
					// null (indicate a NULL Password)
	0x00, 0x09,			// authSize (password authorization session)
	0x40, 0x00, 0x00, 0x09,		// TPM_RS_PW (indicate a password authorization session)
	0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x03,		// count (TPML_PCR_SELECTION)
	0x00, 0x04,			// hash (TPMS_PCR_SELECTION; SHA-1)
	0x03,				// sizeofSelect (TPMS_PCR_SELECTION)
	0x00, 0x00, 0x00,		// pcrSelect (TPMS_PCR_SELECTION; will be set later)
	0x00, 0x0B,			// hash (TPMS_PCR_SELECTION; SHA-256)
	0x03,				// sizeofSelect (TPMS_PCR_SELECTION)
	0x00, 0x00, 0x00,		// pcrSelect (TPMS_PCR_SELECTION; will be set later)
	0x00, 0x0C,			// hash (TPMS_PCR_SELECTION; SHA-384)
	0x03,				// sizeofSelect (TPMS_PCR_SELECTION)
	0x00, 0x00, 0x00		// pcrSelect (TPMS_PCR_SELECTION; will be set later)
};

static const uint8_t tpm2_pcr_read[] = {
	0x80, 0x01,			// TPM_ST_NO_SESSIONS
	0x00, 0x00, 0x00, 0x14,		// commandSize
	0x00, 0x00, 0x01, 0x7E,		// TPM_CC_PCR_Read
	0x00, 0x00, 0x00, 0x01,		// count (TPML_PCR_SELECTION)
	0x00, 0x00,			// hash (TPMS_PCR_SELECTION; will be set later)
	0x03,				// sizeofSelect (TPMS_PCR_SELECTION)
	0x00, 0x00, 0x00		// pcrSelect (TPMS_PCR_SELECTION)
};

static const uint8_t tpm2_pcr_extend[] = {
	0x80, 0x02,			// TPM_ST_SESSIONS
	0x00, 0x00, 0x00, 0x00,		// commandSize (will be set later)
	0x00, 0x00, 0x01, 0x82,		// TPM_CC_PCR_Extend
	0x00, 0x00, 0x00, 0x00,		// {PCR_FIRST:PCR_LAST} (TPMI_DH_PCR)
	0x00, 0x00,			// authSize (NULL Password)
					// null (indicate a NULL Password)
	0x00, 0x09,			// authSize (password authorization session)
	0x40, 0x00, 0x00, 0x09,		// TPM_RS_PW (indicate a password authorization session)
	0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01,		// count (TPML_DIGEST_VALUES)
	0x00, 0x00			// hashAlg (TPMT_HA; will be set later)
					// digest (TPMT_HA; will be added later)
};

static const uint8_t tpm2_pcr_reset[] = {
	0x80, 0x02,			// TPM_ST_SESSIONS
	0x00, 0x00, 0x00, 0x1B,		// commandSize
	0x00, 0x00, 0x01, 0x3D,		// TPM_CC_PCR_Reset
	0x00, 0x00, 0x00, 0x00,		// {PCR_FIRST:PCR_LAST} (TPMI_DH_PCR)
	0x00, 0x00,			// authSize (NULL Password)
					// null (indicate a NULL Password)
	0x00, 0x09,			// authSize (password authorization session)
	0x40, 0x00, 0x00, 0x09,		// TPM_RS_PW (indicate a password authorization session)
	0x00, 0x00, 0x01, 0x00, 0x00
};

#endif /* _ELTT2_H_ */
