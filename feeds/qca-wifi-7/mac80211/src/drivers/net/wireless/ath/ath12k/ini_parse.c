// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "ini.h"

/**
 * ini_file_read() - Function to read the ini file and load it into memory
 * @path: Path to the file
 *
 * Return: Referene pointing to file contenct in memory on success else err ptr
 */
const struct firmware *ini_file_read(const char *path)
{
	const struct firmware *fw;
	int ret;

	ret = firmware_request_nowarn(&fw, path, NULL);

	if (ret) {
		ath12k_dbg(NULL, ATH12K_DBG_INI, "Failed to read file %s", path);
		return ERR_PTR(ret);
	}
	if (fw && !fw->size) {
		release_firmware(fw);
		return ERR_PTR(-ENOENT);
	}

	ath12k_dbg(NULL, ATH12K_DBG_INI, "boot firmware request %s size %zu\n",
		   path, fw->size);

	return fw;
}

/**
 * ini_read_values - Parse the next key-value or section from an INI buffer
 * @main_cursor: Pointer to the current position in the INI buffer.
 * @read_key: Output pointer to the parsed key or section name.
 * @read_value: Output pointer to the parsed value.
 * @section_item: Set to true if the line is a section header, false otherwise.
 *
 * Return: 0 on success,
 *         -EINVAL on syntax error,
 *         -EIO if end of buffer is reached.
 */
static int ini_read_values(char **main_cursor,
			   char **read_key, char **read_value,
				      bool *section_item)
{
	char *cursor = *main_cursor;

	/* foreach line */
	while (*cursor != '\0') {
		char *key = cursor;
		char *value = NULL;
		bool comment = false;
		bool eol = false;

		/*
		 * Look for the end of the line, while noting any
		 * value ('=') or comment ('#') indicators
		 */
		while (!eol) {
			switch (*cursor) {
			case '\r':
			case '\n':
				*cursor = '\0';
				cursor++;
				fallthrough;
			case '\0':
				eol = true;
				break;

			case '=':
				/*
				 * The first '=' is the value indicator.
				 * Subsequent '=' are valid value characters.
				 */
				if (!value && !comment) {
					value = cursor + 1;
					*cursor = '\0';
				}

				cursor++;
				break;

			case '#':
				/*
				 * We don't process comments, so we can null-
				 * terminate unconditionally here (unlike '=').
				 */
				comment = true;
				*cursor = '\0';
				fallthrough;
			default:
				cursor++;
				break;
			}
		}

		key = strim(key);
		/*
		 * Ignoring comments, a valid ini line contains one of:
		 *	1) some 'key=value' config item
		 *	2) section header
		 *	3) a line containing whitespace
		 */
		if (value) {
			*read_key = key;
			*read_value = value;
			*section_item = 0;
			*main_cursor = cursor;
			return 0;
		} else if (key[0] == '[') {
			size_t len = strlen(key);

			if (key[len - 1] != ']') {
				ath12k_dbg(NULL, ATH12K_DBG_INI, "Invalid *.ini syntax '%s'", key);
				return -EINVAL;
			}
			key[len - 1] = '\0';
			*read_key = key + 1;
			*section_item = 1;
			*main_cursor = cursor;
			return 0;
		} else if (key[0] != '\0') {
			ath12k_dbg(NULL, ATH12K_DBG_INI, "Invalid *.ini syntax '%s'", key);
			return -EINVAL;
		}

		/* skip remaining EoL characters */
		while (*cursor == '\n' || *cursor == '\r')
			cursor++;
	}

	return -EIO;
}

int ath12k_ini_parse(const char *ini_path, void *context,
	      ath12k_ini_item_cb item_cb)
{
	int ret = 0;
	char *read_key;
	char *read_value;
	bool section_item;
	int ini_read_count = 0;
	const struct firmware *fw;
	char *cursor;
	char *fbuf;

	fw = ini_file_read(ini_path);

	if (IS_ERR(fw)) {
		ath12k_dbg(NULL, ATH12K_DBG_INI, "Failed to read *.ini file @ %s", ini_path);
		return PTR_ERR(fw);
	}

	fbuf = vmalloc(fw->size + 1);
	if (!fbuf) {
		release_firmware(fw);
		return -ENOMEM;
	}

	memcpy(fbuf, fw->data, fw->size);
	/* Null terminate the buffer */
	fbuf[fw->size] = '\0';
	release_firmware(fw);

	cursor = fbuf;
	while (ini_read_values(&cursor, &read_key, &read_value, &section_item) == 0) {
		if (!section_item) {
			ret = item_cb(context, read_key, read_value);
			if (ret)
				break;
			ini_read_count++;
		} else  {
			ath12k_dbg(NULL, ATH12K_DBG_INI, "Section started in file");
			/* Currently AP Platforms supports and uses Sections,
			 * hence break the loop, sections will be parsed separately,
			 * in case of non AP platforms, sections are used as
			 * logical separators hence continue reading the values.
			 */
			break;
		}
	}

	if (ini_read_count) {
		ath12k_dbg(NULL, ATH12K_DBG_INI, "INI parsed # values read :%d",
			   ini_read_count);
		ret = 0;
	} else {
		ath12k_dbg(NULL, ATH12K_DBG_INI, "INI file parse fail: invalid file format");
		ret = -EINVAL;
	}
	vfree(fbuf);

	return ret;
}

int ath12k_ini_section_parse(const char *ini_path, void *context,
		      ath12k_ini_item_cb item_cb,
				 const char *section_name)
{
	int ret = 0;
	char *read_key;
	char *read_value;
	bool section_item;
	bool section_found = 0;
	bool section_complete = 0;
	int ini_read_count = 0;
	const struct firmware *fw;
	char *cursor;
	char *fbuf;

	fw = ini_file_read(ini_path);
	if (IS_ERR(fw)) {
		ath12k_dbg(NULL, ATH12K_DBG_INI, "Failed to read *.ini file @ %s", ini_path);
		return PTR_ERR(fw);
	}

	fbuf = vmalloc(fw->size + 1);
	if (!fbuf) {
		release_firmware(fw);
		return -ENOMEM;
	}

	memcpy(fbuf, fw->data, fw->size);
	/* Null terminate the buffer */
	fbuf[fw->size] = '\0';
	release_firmware(fw);

	cursor = fbuf;
	while (ini_read_values(&cursor, &read_key, &read_value, &section_item) == 0) {
		if (section_item) {
			if (strcmp(read_key, section_name) == 0) {
				section_found = 1;
				section_complete = 0;
			} else {
				if (section_found == 1)
					section_complete = 1;
				section_found = 0;
			}
		} else if (section_found) {
			ret = item_cb(context, read_key, read_value);
			if (ret)
				break;
			ini_read_count++;
		} else if (section_complete) {
			break;
		}
	}

	ath12k_dbg(NULL, ATH12K_DBG_INI, "Section INI parse read: %d from section %s",
		   ini_read_count, section_name);

	vfree(fbuf);
	return ret;
}
