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

    std::cout << "Walking ..." << std::endl;


    std::string font_dir = "fonts";
    DIR *dir = opendir (font_dir.c_str());
    struct dirent *dirent;
    while (NULL != (dirent = readdir (dir))) {
        std::string d_name = dirent->d_name;
        if (d_name == ".") continue;
        if (d_name == "..") continue;

        std::string full_name = font_dir + "/" + dirent->d_name;

        struct stat sb;
        stat (full_name.c_str(), &sb);
        if (S_ISDIR(sb.st_mode)) {
            boost::icl::interval_set<FT_ULong> set;
            scan (full_name.c_str(), set);

            for (boost::icl::interval_set<FT_ULong>::iterator iter = set.begin();
                 iter != set.end(); ++iter)
            {
            }
        }
    }
    closedir (dir);

    std::cout << "done." << std::endl;

    return 0;
}
