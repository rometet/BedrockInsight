#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <string>

namespace bedrock_insight::sqlite_api {

using ::sqlite3;
using ::sqlite3_stmt;
using ::sqlite3_callback;
using ::sqlite3_destructor_type;

inline constexpr int kOk = 0;
inline constexpr int kRow = 100;
inline constexpr int kDone = 101;
inline constexpr int kReadOnly = 0x00000001;
inline constexpr int kReadWrite = 0x00000002;
inline constexpr int kCreate = 0x00000004;
inline constexpr int kFullMutex = 0x00010000;

class Library final {
public:
    bool load(std::filesystem::path const&, std::string&) { return true; }

    int open_v2(char const*, sqlite3**, int, char const*);
    int close(sqlite3*);
    int exec(sqlite3*, char const*, sqlite3_callback, void*, char**);
    void free(void*);
    char const* errmsg(sqlite3*);
    int busy_timeout(sqlite3*, int);
    int prepare_v2(sqlite3*, char const*, int, sqlite3_stmt**, char const**);
    int finalize(sqlite3_stmt*);
    int step(sqlite3_stmt*);
    int bind_text(sqlite3_stmt*, int, char const*, int, sqlite3_destructor_type);
    int bind_int64(sqlite3_stmt*, int, long long);
    int column_int(sqlite3_stmt*, int);
    long long column_int64(sqlite3_stmt*, int);
    char const* column_text(sqlite3_stmt*, int);

};

} // namespace bedrock_insight::sqlite_api
