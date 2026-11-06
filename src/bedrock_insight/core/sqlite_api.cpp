#include "bedrock_insight/core/sqlite_api.hpp"

namespace bedrock_insight::sqlite_api {

int Library::open_v2(char const* filename, sqlite3** database, int flags, char const* vfs) {
    return sqlite3_open_v2(filename, database, flags, vfs);
}

int Library::close(sqlite3* database) {
    return sqlite3_close(database);
}

int Library::exec(sqlite3* database, char const* sql, sqlite3_callback callback, void* context, char** error) {
    return sqlite3_exec(database, sql, callback, context, error);
}

void Library::free(void* value) {
    sqlite3_free(value);
}

char const* Library::errmsg(sqlite3* database) {
    return sqlite3_errmsg(database);
}

int Library::busy_timeout(sqlite3* database, int milliseconds) {
    return sqlite3_busy_timeout(database, milliseconds);
}

int Library::prepare_v2(sqlite3* database, char const* sql, int length, sqlite3_stmt** statement, char const** tail) {
    return sqlite3_prepare_v2(database, sql, length, statement, tail);
}

int Library::finalize(sqlite3_stmt* statement) {
    return sqlite3_finalize(statement);
}

int Library::step(sqlite3_stmt* statement) {
    return sqlite3_step(statement);
}

int Library::bind_text(sqlite3_stmt* statement, int index, char const* value, int length, sqlite3_destructor_type destructor) {
    return sqlite3_bind_text(statement, index, value, length, destructor);
}

int Library::bind_int64(sqlite3_stmt* statement, int index, long long value) {
    return sqlite3_bind_int64(statement, index, value);
}

int Library::column_int(sqlite3_stmt* statement, int index) {
    return sqlite3_column_int(statement, index);
}

long long Library::column_int64(sqlite3_stmt* statement, int index) {
    return sqlite3_column_int64(statement, index);
}

char const* Library::column_text(sqlite3_stmt* statement, int index) {
    return reinterpret_cast<char const*>(sqlite3_column_text(statement, index));
}

} // namespace bedrock_insight::sqlite_api
