#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <stdint.h>
#include <glibmm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <dirent.h>
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_set.hpp>

FT_Library library;

void scan (const char *path, boost::icl::interval_set<FT_ULong> &set) {
    FTS *fts;
    char * const paths[2] = {(char *)path, 0};

    fts = fts_open (paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    FTSENT *ent;
    while (NULL != (ent = fts_read (fts))) {
        switch (ent->fts_info) {
            case FTS_F:
                FT_Face face;
                int res = FT_New_Face (library, ent->fts_path, 0, &face);
                if (!res) {
                    std::cout << ent->fts_path << std::endl;

                    FT_ULong charcode;
                    FT_UInt gindex;

                    charcode = FT_Get_First_Char (face, &gindex);
                    while (gindex != 0) {
                        set += charcode;
                        charcode = FT_Get_Next_Char (face, charcode, &gindex);
                    }
                }
                break;
        }
    }
}

int main (int argc, char *argv[])
{

    int res;

    res = FT_Init_FreeType (&library);
    if (res) {
        std::cerr << "Oops1." << std::endl;
        return 1;
    }

    DIR *dir = opendir ("fonts");
    struct dirent *dirent;
    while (NULL != (dirent = readdir (dir))) {
        boost::icl::interval_set<FT_ULong> set;
        scan ("/usr/share/fonts", set);
    }
    closedir (dir);



    return 0;
}
