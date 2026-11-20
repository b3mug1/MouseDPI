#pragma once

#include <memory>
#include "core/domain/interfaces/i_settings_repository.hpp"
#include "sqlite_database.hpp"

namespace mc::infra::database {

class SqliteSettingsRepository : public core::domain::ISettingsRepository {
public:
    explicit SqliteSettingsRepository(std::shared_ptr<SqliteDatabase> db);
    ~SqliteSettingsRepository() override = default;

    core::Result<void> save_settings(const core::domain::DeviceSettings& settings) override;

    core::Result<core::domain::DeviceSettings> load_settings() override;

private:
    std::shared_ptr<SqliteDatabase> db_;
};

} // namespace mc::infra::database
