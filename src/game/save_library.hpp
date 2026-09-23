#pragma once
#include "ecosystem.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace terrarium {
struct garden_entry {
    std::string id, name;
    double day{};
    std::array<int, 3> population{};
};

class save_library {
  public:
    explicit save_library(std::filesystem::path root);
    std::vector<garden_entry> list() const;
    std::filesystem::path path(const std::string& id) const;
    bool create(const std::string& name, const world_state& world, std::string& id, std::string& error);
    bool save(const std::string& id, const std::string& name, const world_state& world, std::string& error);
    bool load(const std::string& id, world_state& world, std::string& error) const;
    bool rename(const std::string& id, const std::string& name, std::string& error);
    bool remove(const std::string& id, std::string& error);
    bool import_legacy(std::string& error);
    std::string recent() const;
    void remember(const std::string& id) const;
    const std::filesystem::path& root() const { return root_; }

  private:
    std::filesystem::path root_;

  private:
    bool metadata(const garden_entry& entry, std::string& error) const;
};
}
