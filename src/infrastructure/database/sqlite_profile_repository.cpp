#include "sqlite_profile_repository.hpp"
#include "infrastructure/logging/logger.hpp"
#include <nlohmann/json.hpp>
#include <chrono>

namespace mc::infra::database {

namespace {

std::string serialize_stages(const std::vector<core::domain::DpiStage>& stages) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : stages) {
        arr.push_back({
            {"index", s.index},
            {"dpi", s.dpi},
            {"enabled", s.enabled},
            {"color_rgb", s.color_rgb}
        });
    }
    return arr.dump();
}

std::vector<core::domain::DpiStage> deserialize_stages(const std::string& json_str) {
    std::vector<core::domain::DpiStage> stages;
    try {
        auto parsed = nlohmann::json::parse(json_str);
        if (parsed.is_array()) {
            for (const auto& item : parsed) {
                core::domain::DpiStage s;
                s.index = item.value("index", 0);
                s.dpi = item.value("dpi", 800);
                s.enabled = item.value("enabled", true);
                s.color_rgb = item.value("color_rgb", 0x00FF00);
                stages.push_back(s);
            }
        }
    } catch (...) {}
    return stages;
}

core::domain::MouseProfile row_to_profile(sqlite3_stmt* stmt) {
    core::domain::MouseProfile p;
    p.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const auto* dev_id = sqlite3_column_text(stmt, 2);
    p.device_identifier = dev_id ? reinterpret_cast<const char*>(dev_id) : "";
    p.polling_rate = sqlite3_column_int(stmt, 3);
    p.active_stage_index = sqlite3_column_int(stmt, 4);

    const auto* stages_txt = sqlite3_column_text(stmt, 5);
    if (stages_txt) {
        p.dpi_stages = deserialize_stages(reinterpret_cast<const char*>(stages_txt));
    }
    return p;
}

} // namespace

SqliteProfileRepository::SqliteProfileRepository(std::shared_ptr<SqliteDatabase> db)
    : db_(std::move(db)) {}

core::Result<void> SqliteProfileRepository::save(const core::domain::MouseProfile& profile) {
    if (!db_ || !db_->is_open()) {
        return core::Result<void>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = R"(
        INSERT INTO profiles (id, name, device_identifier, polling_rate, active_stage_index, stages_json, created_at, updated_at)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
        ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            device_identifier = excluded.device_identifier,
            polling_rate = excluded.polling_rate,
            active_stage_index = excluded.active_stage_index,
            stages_json = excluded.stages_json,
            updated_at = excluded.updated_at;
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_->raw_handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return core::Result<void>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()), static_cast<uint32_t>(rc));
    }

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string stages_json = serialize_stages(profile.dpi_stages);

    sqlite3_bind_text(stmt, 1, profile.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, profile.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, profile.device_identifier.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, profile.polling_rate);
    sqlite3_bind_int(stmt, 5, profile.active_stage_index);
    sqlite3_bind_text(stmt, 6, stages_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, now);
    sqlite3_bind_int64(stmt, 8, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return core::Result<void>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()), static_cast<uint32_t>(rc));
    }

    MC_DEBUG("Saved profile: {} ({}) to database", profile.name, profile.id);
    return core::Result<void>::ok();
}

core::Result<std::optional<core::domain::MouseProfile>> SqliteProfileRepository::get_by_id(const std::string& id) {
    if (!db_ || !db_->is_open()) {
        return core::Result<std::optional<core::domain::MouseProfile>>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "SELECT id, name, device_identifier, polling_rate, active_stage_index, stages_json FROM profiles WHERE id = ?1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_->raw_handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return core::Result<std::optional<core::domain::MouseProfile>>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()), static_cast<uint32_t>(rc));
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto p = row_to_profile(stmt);
        sqlite3_finalize(stmt);
        return std::optional<core::domain::MouseProfile>(std::move(p));
    }

    sqlite3_finalize(stmt);
    return std::optional<core::domain::MouseProfile>(std::nullopt);
}

core::Result<std::vector<core::domain::MouseProfile>> SqliteProfileRepository::get_by_device(const std::string& device_id) {
    if (!db_ || !db_->is_open()) {
        return core::Result<std::vector<core::domain::MouseProfile>>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "SELECT id, name, device_identifier, polling_rate, active_stage_index, stages_json FROM profiles WHERE device_identifier = ?1 ORDER BY name ASC;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_->raw_handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return core::Result<std::vector<core::domain::MouseProfile>>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()), static_cast<uint32_t>(rc));
    }

    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<core::domain::MouseProfile> list;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        list.push_back(row_to_profile(stmt));
    }

    sqlite3_finalize(stmt);
    return list;
}

core::Result<std::vector<core::domain::MouseProfile>> SqliteProfileRepository::get_all() {
    if (!db_ || !db_->is_open()) {
        return core::Result<std::vector<core::domain::MouseProfile>>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "SELECT id, name, device_identifier, polling_rate, active_stage_index, stages_json FROM profiles ORDER BY name ASC;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_->raw_handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return core::Result<std::vector<core::domain::MouseProfile>>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()), static_cast<uint32_t>(rc));
    }

    std::vector<core::domain::MouseProfile> list;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        list.push_back(row_to_profile(stmt));
    }

    sqlite3_finalize(stmt);
    return list;
}

core::Result<void> SqliteProfileRepository::remove(const std::string& id) {
    if (!db_ || !db_->is_open()) {
        return core::Result<void>(core::ErrorCode::InvalidOperation, "Database is not open");
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "DELETE FROM profiles WHERE id = ?1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_->raw_handle(), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return core::Result<void>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()), static_cast<uint32_t>(rc));
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return core::Result<void>(core::ErrorCode::IoError, sqlite3_errmsg(db_->raw_handle()), static_cast<uint32_t>(rc));
    }

    MC_INFO("Removed profile id: {}", id);
    return core::Result<void>::ok();
}

} // namespace mc::infra::database
