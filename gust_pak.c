/*
  gust_pak - Archive unpacker for Gust (Koei/Tecmo) PC games
  Copyright © 2019 VitaSmith
  Copyright © 2018 Yuri Hime (shizukachan)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "parson.h"

#pragma pack(push, 1)
typedef struct {
    uint32_t version;
    uint32_t nb_entries;
    uint32_t size;
    uint32_t flags;
} pak_header;

typedef struct {
    char     filename[128];
    uint32_t length;
    uint8_t  key[20];
    uint32_t data_offset;
    uint32_t dummy;
} pak_entry32;

typedef struct {
    char     filename[128];
    uint32_t length;
    uint8_t  key[20];
    uint64_t data_offset;
    uint64_t dummy;
} pak_entry64;
#pragma pack(pop)

static __inline void decode(uint8_t* a, uint8_t* k, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
        a[i] ^= k[i % 20];
}

// To handle either 32 and 64 bit PAK entries
#define entry(i, m) (is_pak32 ? (((pak_entry32*)entries64)[i]).m : entries64[i].m)

int main(int argc, char** argv)
{
    int r = -1;
    FILE* src = NULL;
    uint8_t* buf = NULL;
    char path[256];
    pak_header header;
    pak_entry64* entries64 = NULL;
    JSON_Value* json = NULL;

    if (argc != 2) {
        printf("%s %s (c) 2018-2019 Yuri Hime & VitaSmith\n\n"
            "Usage: %s <Gust PAK file>\n\n"
            "Extract all the files from a Gust .pak archive.\n",
            basename(argv[0]), GUST_TOOLS_VERSION_STR, basename(argv[0]));
        return 0;
    }

    if (is_directory(argv[1])) {
        fprintf(stderr, "ERROR: Directory packing is not supported.\n"
            "To recreate a .pak you need to use the corresponding .json file.\n");
    } else if (strstr(argv[1], ".json") != NULL) {
        fprintf(stderr, "ERROR: Repacking from .json is not supported yet.\n");
    } else {
        printf("Extracting '%s'...\n", argv[1]);
        src = fopen(argv[1], "rb");
        if (src == NULL) {
            fprintf(stderr, "ERROR: Can't open PAK file '%s'", argv[1]);
            goto out;
        }

        if (fread(&header, sizeof(header), 1, src) != 1) {
            fprintf(stderr, "ERROR: Can't read header");
            goto out;
        }

        if ((header.version != 0x20000) || (header.size != sizeof(pak_header)) || (header.flags != 0x0D)) {
            fprintf(stderr, "WARNING: Signature doesn't match expected PAK file format.\n");
        }
        if (header.nb_entries > 16384) {
            fprintf(stderr, "WARNING: More than 16384 entries, is this a supported archive?\n");
        }

        entries64 = calloc(header.nb_entries, sizeof(pak_entry64));
        if (entries64 == NULL) {
            fprintf(stderr, "ERROR: Can't allocate entries\n");
            goto out;
        }

        if (fread(entries64, sizeof(pak_entry64), header.nb_entries, src) != header.nb_entries) {
            fprintf(stderr, "ERROR: Can't read PAK header\n");
            goto out;
        }

        // Detect if we are dealing with 32 or 64-bit pak entries by checking
        // the data_offsets at the expected 32 and 64-bit struct location and
        // adding the absolute value of the difference with last data_offset.
        // The sum that is closest to zero tells us if we are dealing with a
        // 32 or 64-bit PAK archive.
        uint64_t sum[2] = { 0, 0 };
        uint32_t val[2], last[2] = { 0, 0 };
        for (uint32_t i = 0; i < min(header.nb_entries, 64); i++) {
            val[0] = ((pak_entry32*)entries64)[i].data_offset;
            val[1] = (uint32_t)(entries64[i].data_offset >> 32);
            for (int j = 0; j < 2; j++) {
                sum[j] += (val[j] > last[j]) ? val[j] - last[j] : last[j] - val[j];
                last[j] = val[j];
            }
        }
        bool is_pak32 = (sum[0] < sum[1]);
        printf("Detected %s PAK format\n\n", is_pak32 ? "A17/32-bit" : "A18/64-bit");

        // Store the data we'll need to reconstruct the archibe to a JSON file
        json = json_value_init_object();
        json_object_set_string(json_object(json), "name", argv[1]);
        json_object_set_number(json_object(json), "version", header.version);
        json_object_set_number(json_object(json), "flags", header.flags);
        json_object_set_number(json_object(json), "nb_entries", header.nb_entries);
        json_object_set_boolean(json_object(json), "64-bit", !is_pak32);

        bool skip_decode;
        int64_t file_data_offset = sizeof(pak_header) + (int64_t)header.nb_entries * (is_pak32 ? sizeof(pak_entry32) : sizeof(pak_entry64));
        JSON_Value* json_files_array = json_value_init_array();
        printf("OFFSET    SIZE     NAME\n");
        for (uint32_t i = 0; i < header.nb_entries; i++) {
            int j;
            for (j = 0; (j < 20) && (entry(i, key)[j] == 0); j++);
            skip_decode = (j >= 20);
            if (!skip_decode)
                decode((uint8_t*)entry(i, filename), entry(i, key), 128);
            for (size_t n = 0; n < strlen(entry(i, filename)); n++) {
                if (entry(i, filename)[n] == '\\')
                    entry(i, filename)[n] = PATH_SEP;
            }
            printf("%09" PRIx64 " %08x %s%c\n", entry(i, data_offset) + file_data_offset,
                entry(i, length), entry(i, filename), skip_decode ? '*' : ' ');
            JSON_Value* json_file = json_value_init_object();
            json_object_set_string(json_object(json_file), "name", entry(i, filename));
            json_object_set_boolean(json_object(json_file), "skip_decode", skip_decode);
            json_array_append_value(json_array(json_files_array), json_file);
            strcpy(path, &entry(i, filename)[1]);
            for (size_t n = strlen(path); n > 0; n--) {
                if (path[n] == PATH_SEP) {
                    path[n] = 0;
                    break;
                }
            }
            if (!create_path(path)) {
                fprintf(stderr, "ERROR: Can't create path '%s'\n", path);
                goto out;
            }
            fseek64(src, entry(i, data_offset) + file_data_offset, SEEK_SET);
            buf = malloc(entry(i, length));
            if (buf == NULL) {
                fprintf(stderr, "ERROR: Can't allocate entries\n");
                goto out;
            }
            if (fread(buf, 1, entry(i, length), src) != entry(i, length)) {
                fprintf(stderr, "ERROR: Can't read archive\n");
                goto out;
            }
            if (!skip_decode)
                decode(buf, entry(i, key), entry(i, length));
            if (!write_file(buf, entry(i, length), &entry(i, filename)[1], false))
                goto out;
            free(buf);
            buf = NULL;
        }

        json_object_set_value(json_object(json), "files", json_files_array);
        size_t i, len = strlen(argv[1]);
        for (i = len - 1; (argv[1][i] != '.') && (i > 0); i--);
        if (i != 0)
            argv[1][i] = 0;
        strncpy(path, argv[1], sizeof(path) - 1);
        snprintf(path, sizeof(path), "%s.json", argv[1]);
        json_serialize_to_file_pretty(json, path);
        r = 0;
    }

out:
    json_value_free(json);
    free(buf);
    free(entries64);
    if (src != NULL)
        fclose(src);

    if (r != 0) {
        fflush(stdin);
        printf("\nPress any key to continue...");
        (void)getchar();
    }

#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif
    return r;
}
