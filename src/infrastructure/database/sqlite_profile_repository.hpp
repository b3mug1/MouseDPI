#pragma once

#include <memory>
#include "core/domain/interfaces/i_profile_repository.hpp"
#include "sqlite_database.hpp"

namespace mc::infra::database {

class SqliteProfileRepository : public core::domain::IProfileRepository {
public:
    explicit SqliteProfileRepository(std::shared_ptr<SqliteDatabase> db);
    ~SqliteProfileRepository() override = default;

    core::Result<void> save(const core::domain::MouseProfile& profile) override;

    core::Result<std::optional<core::domain::MouseProfile>> get_by_id(const std::string& id) override;

    core::Result<std::vector<core::domain::MouseProfile>> get_by_device(const std::string& device_id) override;

    core::Result<std::vector<core::domain::MouseProfile>> get_all() override;

    core::Result<void> remove(const std::string& id) override;

private:
    std::shared_ptr<SqliteDatabase> db_;
};

} // namespace mc::infra::database
