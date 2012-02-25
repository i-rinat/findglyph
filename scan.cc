#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <glibmm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <dirent.h>
#include <sqlite3.h>
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_set.hpp>

FT_Library library;
const char *dbname = "db.db";

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

void rebuild_db ()
{
    int res;

    res = FT_Init_FreeType (&library);
    if (res) {
        std::cerr << "Oops1." << std::endl;
        return;
    }

    std::cout << "Walking ..." << std::endl;

    sqlite3 *db;

    sqlite3_open (dbname, &db);
    sqlite3_exec (db,
        "DROP TABLE IF EXISTS fonts; "
        "DROP TABLE IF EXISTS glyphs; "
        "CREATE TABLE fonts ( "
        "    id INTEGER PRIMARY KEY, "
        "    name TEXT "
        "); "
        "CREATE TABLE glyphs ( "
        "    font INTEGER, "
        "    c_from TEXT, "
        "    c_to TEXT "
        "); "
        "CREATE UNIQUE INDEX fonts_name ON fonts (name); "
        "CREATE INDEX glyphs_c ON glyphs (c_from, c_to); "
        , NULL, NULL, NULL);
    sqlite3_exec (db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    std::string font_dir = "fonts";
    DIR *dir = opendir (font_dir.c_str());
    struct dirent *dirent;
    while (NULL != (dirent = readdir (dir))) {
        std::string d_name = dirent->d_name;
        if (d_name == ".") continue;
        if (d_name == "..") continue;

        std::string full_name = font_dir + "/" + dirent->d_name;

        sqlite3_stmt *stmt;
        sqlite3_prepare_v2 (db, "INSERT INTO fonts (name) VALUES (?)", -1, &stmt, NULL);
        sqlite3_bind_text (stmt, 1, dirent->d_name, -1, SQLITE_TRANSIENT);
        sqlite3_step (stmt);
        sqlite3_int64 last_row_id = sqlite3_last_insert_rowid (db);
        sqlite3_finalize (stmt);

        sqlite3_prepare_v2 (db, "SELECT id FROM fonts WHERE _rowid_=?", -1, &stmt, NULL);
        sqlite3_bind_int64 (stmt, 1, last_row_id);
        sqlite3_step (stmt);
        int font_id = sqlite3_column_int (stmt, 0);
        sqlite3_finalize (stmt);

        struct stat sb;
        stat (full_name.c_str(), &sb);
        if (S_ISDIR(sb.st_mode)) {
            boost::icl::interval_set<FT_ULong> set;
            scan (full_name.c_str(), set);

            for (boost::icl::interval_set<FT_ULong>::iterator iter = set.begin();
                 iter != set.end(); ++iter)
            {
                Glib::ustring c_from, c_to;
                c_from = (gunichar)iter->lower();
                c_to = (gunichar)iter->upper();

                sqlite3_prepare_v2 (db, "INSERT INTO glyphs (font, c_from, c_to) VALUES (?,?,?)", -1, &stmt, NULL);
                sqlite3_bind_int (stmt, 1, font_id);
                sqlite3_bind_text (stmt, 2, c_from.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text (stmt, 3, c_to.c_str(), -1, SQLITE_STATIC);
                sqlite3_step (stmt);
                sqlite3_finalize (stmt);
            }
        }
    }
    closedir (dir);
    sqlite3_exec (db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_close (db);

    std::cout << "done." << std::endl;
}

void usage (void)
{
    std::cout << "Usage:" << std::endl;
    std::cout << "  findglyph rebuild" << std::endl;
    std::cout << "     rebuilds database. Needs unpacked packages in ./fonts/ directory" << std::endl;
    std::cout << "  findglyph search <...>" << std::endl;
    std::cout << "     searches for packages which has font to render glyph" << std::endl;
}

void search (const Glib::ustring& s)
{
    sqlite3 *db;
    sqlite3_open (dbname, &db);

    std::string sql;

    bool firsttime = true;
    for (unsigned int k = 0; k < s.size(); k ++) {
        if (firsttime) firsttime = false;
        else sql += " INTERSECT ";
        std::stringstream ks;
        ks << (k+1);
        sql += "SELECT name FROM fonts,glyphs WHERE fonts.id=glyphs.font and c_from <= ?";
        sql += ks.str();
        sql += " and ?";
        sql += ks.str();
        sql += " <= c_to";
    }

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2 (db, sql.c_str(), -1, &stmt, NULL);
    for (unsigned int k = 0; k < s.size(); k ++) {
        sqlite3_bind_text (stmt, k + 1, s.substr(k, 1).c_str(), -1, SQLITE_TRANSIENT);
    }

    while (SQLITE_ROW == sqlite3_step (stmt)) {
        std::string name;
        name = (char *) sqlite3_column_text (stmt, 0);
        std::cout << name << std::endl;
    }
    sqlite3_finalize (stmt);
    sqlite3_close (db);
}

int main (int argc, char *argv[])
{
    if (argc == 1) {
        usage();
        _exit (0);
    }

    if (argc >= 2) {
        if (std::string("rebuild") == argv[1]) {
            rebuild_db();
        } else if (std::string("search") == argv[1]) {
            if (argc == 2) {
                usage();
                _exit (0);
            }
            search (argv[2]);
        }
    }
    return 0;
}
