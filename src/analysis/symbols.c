#include "vf2/analysis/symbols.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/analysis/cfg.h"
#include "vf2/file.h"

static vf2_function *find_function(vf2_i960_analysis *analysis, uint32_t address)
{
    size_t index = 0u;
    for (index = 0u; index < analysis->function_count; ++index) {
        if (analysis->functions[index].address == address) {
            return &analysis->functions[index];
        }
    }
    return NULL;
}

static bool parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = 0ul;
    if (text == NULL || value == NULL) {
        return false;
    }
    parsed = strtoul(text, &end, 0);
    if (end == text || (*end != '\0' && *end != '\r' && *end != '\n') || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static void sanitize_name(char *destination, size_t size, const char *source)
{
    size_t output = 0u;
    size_t index = 0u;
    if (size == 0u) {
        return;
    }
    if (source == NULL || source[0] == '\0') {
        destination[0] = '\0';
        return;
    }
    if (isdigit((unsigned char)source[0]) != 0 && output + 1u < size) {
        destination[output++] = '_';
    }
    for (index = 0u; source[index] != '\0' && source[index] != '\r' &&
         source[index] != '\n' && output + 1u < size; ++index) {
        const unsigned char ch = (unsigned char)source[index];
        destination[output++] = (isalnum(ch) != 0 || ch == (unsigned char)'_')
            ? (char)ch
            : '_';
    }
    destination[output] = '\0';
}

static vf2_status apply_file(
    vf2_i960_analysis *analysis,
    const char *path,
    size_t name_column,
    size_t kind_column,
    bool require_function_kind
)
{
    FILE *file = fopen(path, "rb");
    char line[2048];
    if (file == NULL) {
        return VF2_OK;
    }
    if (fgets(line, sizeof(line), file) == NULL) {
        return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *columns[8] = {0};
        size_t count = 0u;
        char *cursor = line;
        uint32_t address = 0u;
        vf2_function *function = NULL;
        while (count < 8u) {
            columns[count++] = cursor;
            cursor = strchr(cursor, ',');
            if (cursor == NULL) {
                break;
            }
            *cursor++ = '\0';
        }
        if (count <= name_column || !parse_u32(columns[0], &address)) {
            continue;
        }
        if (require_function_kind &&
            (count <= kind_column || strcmp(columns[kind_column], "function") != 0)) {
            continue;
        }
        function = find_function(analysis, address);
        if (function != NULL) {
            sanitize_name(function->name, sizeof(function->name), columns[name_column]);
            function->user_named = function->name[0] != '\0';
        }
    }
    return fclose(file) == 0 ? VF2_OK : VF2_ERROR_IO;
}

vf2_status vf2_i960_apply_symbol_overlays(
    vf2_i960_analysis *analysis,
    const char *directory
)
{
    char path[4096];
    vf2_status status = VF2_OK;
    if (analysis == NULL || directory == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_join_path(path, sizeof(path), directory, "functions.csv");
    if (status == VF2_OK) {
        status = apply_file(analysis, path, 2u, 0u, false);
    }
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_join_path(path, sizeof(path), directory, "symbols.csv");
    if (status == VF2_OK) {
        status = apply_file(analysis, path, 1u, 2u, true);
    }
    if (status != VF2_OK) {
        return status;
    }
    status = vf2_join_path(path, sizeof(path), directory, "known_entries.csv");
    if (status == VF2_OK) {
        status = apply_file(analysis, path, 1u, 0u, false);
    }
    if (status != VF2_OK) {
        return status;
    }
    /* Applied last so it wins: these are the shipped i960 symbol names, which
     * are evidence rather than the provisional names the other three files
     * carry. Records that name a label inside a function rather than a
     * function entry simply find no function and are ignored. See
     * docs/ORIGINAL_SYMBOLS.md. */
    status = vf2_join_path(path, sizeof(path), directory, "original_symbols.csv");
    if (status == VF2_OK) {
        status = apply_file(analysis, path, 1u, 0u, false);
    }
    return status;
}

const char *vf2_i960_function_name(
    const vf2_i960_analysis *analysis,
    uint32_t address
)
{
    const vf2_function *function = vf2_i960_find_function(analysis, address);
    return function == NULL ? NULL : function->name;
}
