#include "render/fauna.hpp"
#include "render/fauna_geometry.hpp"
#include <optional>
#include <vector>
namespace terrarium::render {
struct fauna_meshes::impl {
    sengine::scene& scene;
    sengine::scene_asset shells;
    fauna_geometry geometry;
    struct instance {
        sengine::scene_node body;
        std::optional<sengine::scene_instance> shell;
        bool visible{};
    };
    struct pool {
        std::vector<instance> instances;
        std::size_t used{}, previous{};
    };
    std::array<pool, 3> pools;
    void add(pool& pool, species_kind species) {
        auto kind = unsigned(species);
        auto body = create_mesh(scene, geometry.meshes[kind], geometry.materials[kind]);
        std::optional<sengine::scene_instance> shell;
        if (species == species_kind::snail)
            shell = instantiate(scene, shells);
        pool.instances.push_back({body, shell});
    }
    void show(instance& instance, bool enabled) {
        if (instance.visible == enabled)
            return;
        instance.visible = enabled;
        visible(scene, instance.body, enabled);
        if (instance.shell)
            visible(scene, *instance.shell, enabled);
    }
};
fauna_meshes::fauna_meshes(sengine::scene& scene, sengine::scene_asset shells,
                           const std::filesystem::path& directory)
    : impl_(std::make_unique<impl>(scene, shells, load_fauna(scene, directory))) {}
fauna_meshes::~fauna_meshes() = default;
void fauna_meshes::place(species_kind species, const sengine::mat4& transform,
                         const presentation::animal_weights& weights) {
    auto& p = *impl_;
    auto& pool = p.pools[unsigned(species)];
    if (pool.instances.size() <= pool.used)
        p.add(pool, species);
    auto& instance = pool.instances[pool.used++];
    p.show(instance, true);
    set_transform(p.scene, instance.body, transform);
    if (instance.shell)
        set_transform(p.scene, root(p.scene, *instance.shell), transform);
    morph_weights(p.scene, instance.body, weights);
}
void fauna_meshes::finish() {
    auto& p = *impl_;
    for (auto& pool : p.pools) {
        for (std::size_t i = pool.used; i < pool.previous; ++i)
            p.show(pool.instances[i], false);
        pool.previous = pool.used;
        pool.used = 0;
    }
}
}
