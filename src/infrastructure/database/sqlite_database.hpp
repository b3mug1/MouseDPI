#pragma once

#include <string>
#include <memory>
#include <mutex>
#include "core/types.hpp"
#include <sqlite3.h>

namespace mc::infra::database {

class SqliteDatabase {
public:
    explicit SqliteDatabase(std::string db_path);
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&&) noexcept;
    SqliteDatabase& operator=(SqliteDatabase&&) noexcept;

    core::Result<void> initialize();

    [[nodiscard]] sqlite3* raw_handle() const noexcept {
        return db_;
    }

    [[nodiscard]] bool is_open() const noexcept {
        return db_ != nullptr;
    }

    core::Result<void> execute(const std::string& sql);

    std::mutex& mutex() noexcept {
        return db_mutex_;
    }

private:
    core::Result<void> run_migrations();

    std::string db_path_;
    sqlite3* db_{nullptr};
    mutable std::mutex db_mutex_;
};

} // namespace mc::infra::database
