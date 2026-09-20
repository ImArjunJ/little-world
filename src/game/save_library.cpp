#include "save_library.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace terrarium {
namespace {
bool valid_id(const std::string& id) {
    return id.size() >= 8 && id.size() <= 30 && id.starts_with("garden-") &&
           std::all_of(id.begin() + 7, id.end(), [](char c) { return c >= '0' && c <= '9'; });
}
bool valid_name(const std::string& name) {
    return !name.empty() && name.size() <= 64 &&
           std::any_of(name.begin(), name.end(), [](unsigned char c) { return c > 32; }) &&
           std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
}
save_library::save_library(std::filesystem::path root) : root_(std::move(root)) {
    std::filesystem::create_directories(root_ / "gardens");
}
std::filesystem::path save_library::path(const std::string& id) const {
    if (!valid_id(id))
        throw std::invalid_argument("Invalid garden identifier.");
    return root_ / "gardens" / id / "world.save";
}
std::vector<garden_entry> save_library::list() const {
    std::vector<garden_entry> result;
    for (const auto& dir : std::filesystem::directory_iterator(root_ / "gardens")) {
        auto id = dir.path().filename().string();
        if (!dir.is_directory() || !valid_id(id) || !std::filesystem::exists(path(id)))
            continue;
        garden_entry e;
        e.id = id;
        e.name = "Untitled garden";
        std::ifstream in(dir.path() / "garden.info");
        std::string name;
        if (in >> std::quoted(name) >> e.day >> e.population[0] >> e.population[1] >> e.population[2])
            if (valid_name(name))
                e.name = name;
        result.push_back(e);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.id > b.id; });
    return result;
}
bool save_library::metadata(const garden_entry& e, std::string& error) const {
    try {
        if (!valid_name(e.name))
            throw std::runtime_error("Give your garden a name of 1 to 64 bytes.");
        auto file = path(e.id).parent_path() / "garden.info";
        auto temp = file;
        temp += ".tmp";
        std::ofstream out(temp);
        out << std::quoted(e.name) << ' ' << std::setprecision(17) << e.day;
        for (int p : e.population)
            out << ' ' << p;
        out << '\n';
        out.close();
        if (!out)
            throw std::runtime_error("Could not write the garden label.");
        std::filesystem::rename(temp, file);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
bool save_library::save(const std::string& id, const std::string& name, const world_state& world,
                        std::string& error) {
    if (!valid_name(name) || !valid_id(id)) {
        error = "Invalid garden name or identifier.";
        return false;
    }
    if (!world.save(path(id), error))
        return false;
    return metadata({id, name, world.day(), world.populations()}, error);
}
bool save_library::create(const std::string& name, const world_state& world, std::string& id,
                          std::string& error) {
    if (!valid_name(name)) {
        error = "Give your garden a name of 1 to 64 bytes.";
        return false;
    }
    try {
        std::string candidate;
        for (unsigned i = 1;; ++i) {
            std::ostringstream number;
            number << "garden-" << std::setw(8) << std::setfill('0') << i;
            candidate = number.str();
            if (std::filesystem::create_directory(path(candidate).parent_path()))
                break;
        }
        if (!save(candidate, name, world, error))
            return false;
        id = candidate;
        remember(id);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
bool save_library::load(const std::string& id, world_state& world, std::string& error) const {
    if (!valid_id(id)) {
        error = "Invalid garden identifier.";
        return false;
    }
    return world.load(path(id), error);
}
bool save_library::rename(const std::string& id, const std::string& name, std::string& error) {
    for (auto entry : list())
        if (entry.id == id) {
            entry.name = name;
            return metadata(entry, error);
        }
    error = "This garden is no longer available.";
    return false;
}
bool save_library::remove(const std::string& id, std::string& error) {
    try {
        const auto directory = path(id).parent_path();
        const auto status = std::filesystem::symlink_status(directory);
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
            error = "This garden is no longer available.";
            return false;
        }
        const bool was_recent = recent() == id;
        std::filesystem::remove_all(directory);
        if (was_recent) {

            std::error_code ignored;
            std::filesystem::remove(root_ / "recent-garden", ignored);
            const auto remaining = list();
            if (!remaining.empty())
                remember(remaining.front().id);
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
bool save_library::import_legacy(std::string& error) {
    auto legacy = root_ / "garden.save";
    auto marker = root_ / "legacy-imported";
    if (!std::filesystem::exists(legacy) || std::filesystem::exists(marker))
        return true;

    std::string id = "garden-00000000";
    if (!std::filesystem::exists(path(id))) {
        world_state world(1, false);
        if (!world.load(legacy, error))
            return false;
        std::filesystem::create_directories(path(id).parent_path());
        if (!save(id, "My first little world", world, error))
            return false;
    }
    std::ofstream out(marker);
    out << id << '\n';
    if (!out) {
        error = "Could not record the imported garden.";
        return false;
    }
    if (recent().empty())
        remember(id);
    return true;
}
std::string save_library::recent() const {
    std::ifstream in(root_ / "recent-garden");
    std::string id;
    in >> id;
    return valid_id(id) && std::filesystem::exists(path(id)) ? id : std::string{};
}
void save_library::remember(const std::string& id) const {
    if (!valid_id(id))
        return;
    auto temp = root_ / "recent-garden.tmp";
    std::ofstream out(temp);
    out << id << '\n';
    out.close();
    if (out) {
        std::error_code ec;
        std::filesystem::rename(temp, root_ / "recent-garden", ec);
    }
}
}
