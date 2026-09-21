#include "render/fauna.hpp"
#include "render/fauna_geometry.hpp"
#include <filament/Engine.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/TransformManager.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/FilamentInstance.h>
#include <stdexcept>
#include <utils/EntityManager.h>
#include <vector>

namespace terrarium::render {
using namespace filament;
struct fauna_meshes::impl {
    Engine& engine;
    Scene& scene;
    gltfio::AssetLoader& loader;
    gltfio::FilamentAsset* asset;
    struct instance {
        utils::Entity body;
        gltfio::FilamentInstance* shell{};
        bool visible{};
    };
    struct pool {
        std::vector<instance> instances;
        size_t used{}, previous{};
    };
    std::array<pool, 3> pools;
    std::unique_ptr<fauna_geometry> geometry;
    impl(Engine& e, Scene& s, gltfio::AssetLoader& l, gltfio::FilamentAsset* a)
        : engine(e), scene(s), loader(l), asset(a) {}

    void add(pool& pool, species_kind species) {
        gltfio::FilamentInstance* shell = nullptr;
        if (species == species_kind::snail) {
            shell = pool.instances.empty() ? asset->getInstance() : loader.createInstance(asset);
            if (!shell)
                throw std::runtime_error("Cannot create snail shell instance");
        }
        auto body = utils::EntityManager::get().create();
        pool.instances.push_back({body, shell});
        engine.getTransformManager().create(body);
        geometry->build(body, species);
    }
    void show(instance& instance, bool visible) {
        if (instance.visible == visible)
            return;
        instance.visible = visible;
        if (visible) {
            scene.addEntity(instance.body);
            if (instance.shell)
                scene.addEntities(instance.shell->getEntities(), instance.shell->getEntityCount());
        } else {
            scene.remove(instance.body);
            if (instance.shell)
                scene.removeEntities(instance.shell->getEntities(), instance.shell->getEntityCount());
        }
    }
    ~impl() {
        for (auto& pool : pools)
            for (auto& instance : pool.instances) {
                scene.remove(instance.body);
                engine.destroy(instance.body);
                utils::EntityManager::get().destroy(instance.body);
            }
        scene.removeEntities(asset->getEntities(), asset->getEntityCount());
        loader.destroyAsset(asset);
    }
};
fauna_meshes::fauna_meshes(Engine& engine, Scene& scene, gltfio::AssetLoader& loader,
                           gltfio::FilamentAsset* asset, const std::filesystem::path& directory)
    : impl_(std::make_unique<impl>(engine, scene, loader, asset)) {
    impl_->geometry = std::make_unique<fauna_geometry>(engine, directory);
}
fauna_meshes::~fauna_meshes() = default;
void fauna_meshes::place(species_kind species, const math::mat4f& transform,
                         const presentation::animal_weights& weights) {
    auto& p = *impl_;
    auto& pool = p.pools[unsigned(species)];
    if (pool.instances.size() <= pool.used)
        p.add(pool, species);
    auto& instance = pool.instances[pool.used++];
    p.show(instance, true);
    auto& transforms = p.engine.getTransformManager();
    transforms.setTransform(transforms.getInstance(instance.body), transform);
    if (instance.shell)
        transforms.setTransform(transforms.getInstance(instance.shell->getRoot()), transform);
    auto& renderables = p.engine.getRenderableManager();
    renderables.setMorphWeights(renderables.getInstance(instance.body), weights.data(), weights.size());
}
void fauna_meshes::finish() {
    auto& p = *impl_;
    for (auto& pool : p.pools) {
        for (size_t i = pool.used; i < pool.previous; ++i)
            p.show(pool.instances[i], false);
        pool.previous = pool.used;
        pool.used = 0;
    }
}
}
