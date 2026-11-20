#include "sqlite_settings_repository.hpp"
#include "infrastructure/logging/logger.hpp"

namespace mc::infra::database {

SqliteSettingsRepository::SqliteSettingsRepository(std::shared_ptr<SqliteDatabase> db)
    : db_(std::move(db)) {}

core::Result<void> SqliteSettingsRepository::save_settings(const core::domain::DeviceSettings& settings) {
    if (!db_ || !db_->is_open()) {
        return core::Result<void>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    auto set_kv = [this](const char* key, const std::string& val) -> bool {
        const char* sql = "INSERT INTO app_settings (key, value) VALUES (?1, ?2) ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_->raw_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, val.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return ok;
    };

    set_kv("start_with_windows", settings.start_with_windows ? "1" : "0");
    set_kv("minimize_to_tray", settings.minimize_to_tray ? "1" : "0");
    set_kv("close_to_tray", settings.close_to_tray ? "1" : "0");
    set_kv("show_notifications", settings.show_notifications ? "1" : "0");
    set_kv("default_profile_id", settings.default_profile_id);
    set_kv("active_device_id", settings.active_device_id);

    MC_DEBUG("Saved application settings to database");
    return core::Result<void>::ok();
}

core::Result<core::domain::DeviceSettings> SqliteSettingsRepository::load_settings() {
    if (!db_ || !db_->is_open()) {
        return core::Result<core::domain::DeviceSettings>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    core::domain::DeviceSettings s;

    const char* sql = "SELECT key, value FROM app_settings;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->raw_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return core::Result<core::domain::DeviceSettings>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const auto* val_txt = sqlite3_column_text(stmt, 1);
        std::string val = val_txt ? reinterpret_cast<const char*>(val_txt) : "";

        if (key == "start_with_windows") s.start_with_windows = (val == "1" || val == "true");
        else if (key == "minimize_to_tray") s.minimize_to_tray = (val == "1" || val == "true");
        else if (key == "close_to_tray") s.close_to_tray = (val == "1" || val == "true");
        else if (key == "show_notifications") s.show_notifications = (val == "1" || val == "true");
        else if (key == "default_profile_id") s.default_profile_id = val;
        else if (key == "active_device_id") s.active_device_id = val;
    }

    sqlite3_finalize(stmt);
    return s;
}

} // namespace mc::infra::database
