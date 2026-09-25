#include "render/fauna.hpp"
#include "render/fauna_geometry.hpp"
#include "sengine/world_renderer.hpp"
namespace terrarium::render {
struct fauna_meshes::impl {
  public:
    impl(sengine::scene& resources, const std::filesystem::path& directory)
        : shells(resources, directory / "fauna.glb"), geometry(load_fauna(resources, directory)),
          drawings(entities, resources) {}

  private:
    struct instance {
        sengine::entity body;
        std::unique_ptr<sengine::model_instance> shell;
    };
    struct pool {
        std::vector<instance> instances;
        std::size_t used{}, previous{};
    };

  public:
    void add(pool& pool, species_kind species) {
        const auto kind = unsigned(species);
        const auto body = entities.create();
        drawings.mesh(body, geometry.meshes[kind], geometry.materials[kind]);
        std::unique_ptr<sengine::model_instance> shell;
        if (species == species_kind::snail)
            shell = std::make_unique<sengine::model_instance>(shells);
        pool.instances.push_back({body, std::move(shell)});
    }
    void show(instance& instance, bool enabled) {
        drawings.show(instance.body, enabled);
        if (instance.shell)
            instance.shell->visible(enabled);
    }

  public:
    sengine::model shells;
    fauna_geometry geometry;
    sengine::world entities;
    sengine::world_renderer drawings;
    std::array<pool, 3> pools;
};
fauna_meshes::fauna_meshes(sengine::scene& scene, const std::filesystem::path& directory)
    : impl_(std::make_unique<impl>(scene, directory)) {}
fauna_meshes::~fauna_meshes() = default;
void fauna_meshes::place(species_kind species, const sengine::mat4& transform,
                         const presentation::animal_weights& weights) {
    auto& p = *impl_;
    auto& pool = p.pools[unsigned(species)];
    if (pool.instances.size() <= pool.used)
        p.add(pool, species);
    auto& instance = pool.instances[pool.used++];
    p.show(instance, true);
    p.entities.set_local_transform(instance.body, transform);
    if (instance.shell)
        instance.shell->transform(transform);
    p.drawings.morph_weights(instance.body, weights);
}
void fauna_meshes::finish() {
    auto& p = *impl_;
    for (auto& pool : p.pools) {
        for (std::size_t i = pool.used; i < pool.previous; ++i)
            p.show(pool.instances[i], false);
        for (std::size_t i = 0; i < std::max(pool.used, pool.previous); ++i)
            if (auto& shell = pool.instances[i].shell)
                shell->synchronize();
        pool.previous = pool.used;
        pool.used = 0;
    }
    p.drawings.synchronize();
}
}
