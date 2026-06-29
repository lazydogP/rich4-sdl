#include <stdio.h>
#include <stdlib.h>

#include "data/map_data.h"
#include "resource/mkf.h"

int main(int argc, char **argv)
{
    Rich4Mkf *map_mkf = NULL;
    Rich4Mkf *mapdat_mkf = NULL;
    int has_mapdat;
    int gmap;
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: mkf_validate_map_data <Map.mkf> [Mapdat.mkf]\n");
        return 2;
    }
    if (rich4_mkf_open(argv[1], &map_mkf) != RICH4_MKF_OK) {
        rich4_mkf_close(map_mkf);
        return 1;
    }
    has_mapdat = argc == 3;
    if (has_mapdat && rich4_mkf_open(argv[2], &mapdat_mkf) != RICH4_MKF_OK) {
        rich4_mkf_close(mapdat_mkf);
        rich4_mkf_close(map_mkf);
        return 1;
    }
    for (gmap = 0; gmap < 8; gmap++) {
        MapDataSelection selection = rich4_map_data_select(gmap / 4, gmap % 4, has_mapdat);
        Rich4Mkf *archive = selection.archive == MAP_DATA_ARCHIVE_MAPDAT ? mapdat_mkf : map_mkf;
        unsigned char *data = NULL;
        size_t size = 0;
        MapData parsed;
        Rich4MkfError read_error = rich4_mkf_read_resource(archive,
                                                           selection.resource_index,
                                                           &data,
                                                           &size,
                                                           NULL);
        MapDataError parse_error = read_error == RICH4_MKF_OK
            ? rich4_map_data_parse(data,
                                   size,
                                   selection.archive,
                                   selection.resource_index,
                                   &parsed)
            : MAP_DATA_ERR_RANGE;
        free(data);
        if (parse_error != MAP_DATA_OK) {
            fprintf(stderr, "gmap=%d archive=%s resource=%lu error=%s\n",
                    gmap,
                    selection.archive == MAP_DATA_ARCHIVE_MAPDAT ? "MAPDAT" : "MAP",
                    (unsigned long)selection.resource_index,
                    rich4_map_data_error_name(parse_error));
            rich4_mkf_close(mapdat_mkf);
            rich4_mkf_close(map_mkf);
            return 1;
        }
        printf("gmap=%d archive=%s resource=%lu routes=%lu lands=%lu facilities=%lu auxiliary=%lu specials=%lu\n",
               gmap,
               selection.archive == MAP_DATA_ARCHIVE_MAPDAT ? "MAPDAT" : "MAP",
               (unsigned long)selection.resource_index,
               (unsigned long)parsed.route_count,
               (unsigned long)parsed.land_count,
               (unsigned long)parsed.facility_count,
               (unsigned long)parsed.auxiliary_count,
               (unsigned long)parsed.special_count);
        rich4_map_data_free(&parsed);
    }
    rich4_mkf_close(mapdat_mkf);
    rich4_mkf_close(map_mkf);
    return 0;
}
