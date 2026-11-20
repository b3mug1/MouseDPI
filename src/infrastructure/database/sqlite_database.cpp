#include "sqlite_database.hpp"
#include "infrastructure/logging/logger.hpp"

namespace mc::infra::database {

SqliteDatabase::SqliteDatabase(std::string db_path) : db_path_(std::move(db_path)) {}

SqliteDatabase::~SqliteDatabase() {
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

SqliteDatabase::SqliteDatabase(SqliteDatabase&& other) noexcept
    : db_path_(std::move(other.db_path_)), db_(other.db_) {
    other.db_ = nullptr;
}

SqliteDatabase& SqliteDatabase::operator=(SqliteDatabase&& other) noexcept {
    if (this != &other) {
        if (db_) {
            sqlite3_close_v2(db_);
        }
        db_path_ = std::move(other.db_path_);
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

core::Result<void> SqliteDatabase::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (db_) {
        return core::Result<void>::ok();
    }

    int rc = sqlite3_open_v2(
        db_path_.c_str(),
        &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (rc != SQLITE_OK) {
        std::string err_msg = db_ ? sqlite3_errmsg(db_) : "Failed to open sqlite database";
        MC_ERROR("SQLite open failed: {} (code: {})", err_msg, rc);
        if (db_) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }
        return core::Result<void>(core::ErrorCode::IoError, err_msg, static_cast<uint32_t>(rc));
    }

    // Enable WAL mode for high performance and low lock contention
    char* err_str = nullptr;
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &err_str);
    if (err_str) {
        sqlite3_free(err_str);
    }
    sqlite3_exec(db_, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);

    auto mig_res = run_migrations();
    if (!mig_res) {
        return mig_res;
    }

    MC_INFO("SQLite database initialized successfully at: {}", db_path_);
    return core::Result<void>::ok();
}

core::Result<void> SqliteDatabase::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (!db_) {
        return core::Result<void>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? err_msg : "SQLite execution error";
        if (err_msg) sqlite3_free(err_msg);
        MC_ERROR("SQLite error executing [{}]: {}", sql, msg);
        return core::Result<void>(core::ErrorCode::IoError, msg, static_cast<uint32_t>(rc));
    }

    return core::Result<void>::ok();
}

core::Result<void> SqliteDatabase::run_migrations() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS profiles (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            device_identifier TEXT,
            polling_rate INTEGER NOT NULL,
            active_stage_index INTEGER NOT NULL,
            stages_json TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS app_settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, schema, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? err_msg : "Failed to run migrations";
        if (err_msg) sqlite3_free(err_msg);
        MC_ERROR("SQLite migration failed: {}", msg);
        return core::Result<void>(core::ErrorCode::IoError, msg, static_cast<uint32_t>(rc));
    }

    return core::Result<void>::ok();
}

} // namespace mc::infra::database
