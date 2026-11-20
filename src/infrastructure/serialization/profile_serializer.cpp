#include "profile_serializer.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

namespace mc::infra::serialization {

std::string ProfileSerializer::to_json(const core::domain::MouseProfile& profile, bool pretty) {
    nlohmann::json j;
    j["name"] = profile.name;
    if (!profile.id.empty()) {
        j["id"] = profile.id;
    }
    if (!profile.device_identifier.empty()) {
        j["device_identifier"] = profile.device_identifier;
    }

    nlohmann::json stages_arr = nlohmann::json::array();
    int active_dpi = 800;

    for (size_t i = 0; i < profile.dpi_stages.size(); ++i) {
        stages_arr.push_back(profile.dpi_stages[i].dpi);
        if (static_cast<int>(i) == profile.active_stage_index) {
            active_dpi = profile.dpi_stages[i].dpi;
        }
    }

    j["dpi_stages"] = stages_arr;
    j["active_dpi"] = active_dpi;
    j["polling_rate"] = profile.polling_rate;

    return pretty ? j.dump(4) : j.dump();
}

core::Result<core::domain::MouseProfile> ProfileSerializer::from_json(const std::string& json_str) {
    try {
        auto j = nlohmann::json::parse(json_str);
        if (!j.is_object()) {
            return core::Result<core::domain::MouseProfile>(
                core::ErrorCode::InvalidData, "Profile JSON root must be an object");
        }

        core::domain::MouseProfile p;
        p.name = j.value("name", "Imported Profile");
        p.id = j.value("id", "");
        p.device_identifier = j.value("device_identifier", "");
        p.polling_rate = j.value("polling_rate", 1000);

        int active_dpi = j.value("active_dpi", 800);

        if (j.contains("dpi_stages") && j["dpi_stages"].is_array()) {
            int idx = 0;
            for (const auto& item : j["dpi_stages"]) {
                core::domain::DpiStage stage;
                stage.index = idx;
                if (item.is_number()) {
                    stage.dpi = item.get<int>();
                    stage.enabled = true;
                } else if (item.is_object()) {
                    stage.dpi = item.value("dpi", 800);
                    stage.enabled = item.value("enabled", true);
                    stage.color_rgb = item.value("color_rgb", 0x00FF00);
                }

                if (stage.dpi == active_dpi) {
                    p.active_stage_index = idx;
                }
                p.dpi_stages.push_back(stage);
                idx++;
            }
        }

        if (p.dpi_stages.empty()) {
            p.dpi_stages.push_back({0, 800, true, 0x00FF00});
        }

        return p;
    } catch (const nlohmann::json::exception& ex) {
        return core::Result<core::domain::MouseProfile>(
            core::ErrorCode::InvalidData, std::string("JSON parsing error: ") + ex.what());
    }
}

} // namespace mc::infra::serialization
