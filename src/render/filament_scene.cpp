#include "render/filament_scene.hpp"
#include "animal_motion.hpp"
#include "carry_pose.hpp"
#include "habitat_surface.hpp"
#include "render/environment.hpp"
#include "render/fauna.hpp"
#include "render/substrate.hpp"
#include "render/vessel_glass.hpp"
#include "sengine/image_export.hpp"
#include "sengine/renderer.hpp"
#include <backend/PixelBufferDescriptor.h>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/LightManager.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/TransformManager.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <fstream>
#include <gltfio/Animator.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/FilamentInstance.h>
#include <gltfio/MaterialProvider.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <gltfio/materials/uberarchive.h>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utils/EntityManager.h>
#include <utils/NameComponentManager.h>
#include <vector>

namespace terrarium::render {
using namespace filament;
struct filament_scene::impl {
    sengine::renderer graphics;
    Engine* engine;
    Scene* scene;
    View* view;
    Camera* camera;
    impl(void* window, void* shared_context)
        : graphics(window, shared_context), engine(&graphics.engine()), scene(&graphics.scene()),
          view(&graphics.view()), camera(&graphics.camera()) {}
    std::unique_ptr<environment_lighting> environment;
    std::unique_ptr<substrate_meshes> substrate;
    std::unique_ptr<fauna_meshes> fauna;
    std::unique_ptr<vessel_glass> glass;
    utils::Entity sun{};
    gltfio::MaterialProvider* materials{};
    gltfio::AssetLoader* loader{};
    gltfio::FilamentAsset* asset{};
    gltfio::FilamentAsset* gardens{};
    gltfio::FilamentAsset* character{};
    std::unique_ptr<sengine::native_hud> hud;
    std::optional<std::tuple<int, bool, bool, bool>> settings;
    float near_plane{.045f};
    sengine::point player_head{};
    bool outside_body{};
    struct named {
        utils::Entity entity;
        math::mat4f bind;
        std::vector<utils::Entity> renderables{};
        bool visible{};
    };
    void collect(named& n, utils::Entity entity) {
        auto& tm = engine->getTransformManager();
        if (engine->getRenderableManager().hasComponent(entity))
            n.renderables.push_back(entity);
        for (auto child : tm.getChildrenRange(tm.getInstance(entity)))
            collect(n, tm.getEntity(child));
    }
    void show(named& n, bool visible) {
        if (n.visible == visible)
            return;
        n.visible = visible;
        if (visible)
            scene->addEntities(n.renderables.data(), n.renderables.size());
        else
            scene->removeEntities(n.renderables.data(), n.renderables.size());
    }
    std::map<std::string, named> joints;
    std::array<utils::Entity, 10> jars{}, markers{};
    std::array<std::array<named, 4>, 10> vessels{};
    std::array<math::mat4f, 10> jar_transforms{};
    std::array<named, 2> boots{};
    std::array<std::array<named, 320>, 10> plants{};
    std::array<std::array<std::array<named, 3>, 320>, 10> foliage{};
    std::array<unsigned, 10> previous_plants{};
    std::array<float, 3> plant_radius{};
    std::array<presentation::animal_motion, 10> animal_motion;
    std::array<named, 6> previews{};
    std::map<std::string, size_t> clips;
    size_t active_clip{}, previous_clip{};
    float clip_time{}, previous_clip_time{}, blend_time{1};
    bool clip_started{};
    std::array<math::mat4f, 2> boot_attachments{};
    gltfio::FilamentAsset* load(const std::filesystem::path& path, bool add = true) {
        std::ifstream in(path, std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        if (bytes.empty())
            throw std::runtime_error("Missing content: " + path.string());
        auto* result = loader->createAsset(bytes.data(), uint32_t(bytes.size()));
        if (!result)
            throw std::runtime_error("Invalid content: " + path.string());
        const auto source_path = path.string();
        gltfio::ResourceConfiguration config{};
        config.gltfPath = source_path.c_str();
        config.engine = engine;
        config.normalizeSkinningWeights = true;
        gltfio::ResourceLoader resources(config);
        resources.addTextureProvider("image/png", textures.get());
        resources.addTextureProvider("image/jpeg", textures.get());
        for (size_t i = 0; i < result->getResourceUriCount(); ++i) {
            auto* uri = result->getResourceUris()[i];
            std::ifstream source(path.parent_path() / uri, std::ios::binary);
            if (!source)
                throw std::runtime_error("Missing character resource");
            auto* data = new std::vector<uint8_t>((std::istreambuf_iterator<char>(source)),
                                                  std::istreambuf_iterator<char>{});
            resources.addResourceData(
                uri, gltfio::ResourceLoader::BufferDescriptor(
                         data->data(), data->size(),
                         [](void*, size_t, void* u) { delete static_cast<std::vector<uint8_t>*>(u); }, data));
        }
        if (!resources.loadResources(result)) {
            loader->destroyAsset(result);
            throw std::runtime_error("Content upload failed: " + path.string());
        }
        if (add)
            scene->addEntities(result->getEntities(), result->getEntityCount());
        return result;
    }
    std::unique_ptr<utils::NameComponentManager> names;
    std::unique_ptr<gltfio::TextureProvider> textures;
    ~impl() {
        if (!engine)
            return;
        engine->flushAndWait();
        hud.reset();
        fauna.reset();
        substrate.reset();
        environment.reset();
        if (asset)
            loader->destroyAsset(asset);
        if (gardens)
            loader->destroyAsset(gardens);
        if (character)
            loader->destroyAsset(character);
        glass.reset();
        if (loader)
            gltfio::AssetLoader::destroy(&loader);
        textures.reset();
        if (materials) {
            materials->destroyMaterials();
            delete materials;
        }
        if (sun) {
            engine->destroy(sun);
            utils::EntityManager::get().destroy(sun);
        }
    }
};

filament_scene::filament_scene(void* window, const std::filesystem::path& path, render_quality quality,
                               void* shared_context)
    : impl_(std::make_unique<impl>(window, shared_context)) {
    auto& p = *impl_;
    auto& e = *p.engine;
    p.graphics.backend().setClearOptions({.clearColor = {0.25f, 0.32f, 0.36f, 1}, .clear = true});
    p.view->setVisibleLayers(0xff, 0x01);
    p.camera->setExposure(8.0f, 1.0f / 90.0f, 100.0f);
    TemporalAntiAliasingOptions taa;
    taa.enabled = quality == render_quality::high;
    p.view->setTemporalAntiAliasingOptions(taa);
    p.view->setAntiAliasing(View::AntiAliasing::FXAA);
    AmbientOcclusionOptions ao;
    ao.enabled = quality == render_quality::high;
    ao.radius = 0.35f;
    ao.power = 1.2f;
    ao.quality = QualityLevel::HIGH;
    ao.resolution = 1.0f;
    ao.lowPassFilter = QualityLevel::HIGH;
    p.view->setAmbientOcclusionOptions(ao);
    p.view->setDithering(View::Dithering::TEMPORAL);
    p.view->setShadowType(quality == render_quality::high ? ShadowType::PCSS : ShadowType::PCF);
    p.sun = utils::EntityManager::get().create();
    LightManager::ShadowOptions shadow;
    shadow.mapSize = quality == render_quality::high ? 2048 : 1024;
    shadow.shadowCascades = quality == render_quality::high ? 3 : 2;
    shadow.cascadeSplitPositions[0] = .08f;
    shadow.cascadeSplitPositions[1] = .30f;
    shadow.shadowFar = quality == render_quality::high ? 90.0f : 45.0f;
    shadow.shadowFarHint = 80.0f;
    shadow.stable = true;
    shadow.lispsm = false;
    LightManager::Builder(LightManager::Type::SUN)
        .color({1.0f, 0.91f, 0.76f})
        .intensity(28000)
        .direction({0.42f, -0.46f, 0.78f})
        .sunAngularRadius(.27f)
        .castShadows(true)
        .shadowOptions(shadow)
        .build(e, p.sun);
    p.scene->addEntity(p.sun);
    p.environment = std::make_unique<environment_lighting>(e, *p.scene);
    FogOptions fog;
    fog.enabled = true;
    fog.distance = 55;
    fog.density = .003f;
    fog.cutOffDistance = 320;
    fog.heightFalloff = .035f;
    fog.color = {.60f, .72f, .79f};
    p.view->setFogOptions(fog);
    p.materials = gltfio::createUbershaderProvider(&e, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);
    p.names = std::make_unique<utils::NameComponentManager>(utils::EntityManager::get());
    p.loader = gltfio::AssetLoader::create({.engine = &e, .materials = p.materials, .names = p.names.get()});
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Scene file missing: " + path.string());
    const auto length = file.tellg();
    if (length <= 0 || uint64_t(length) > UINT32_MAX)
        throw std::runtime_error("Scene size is invalid");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!file)
        throw std::runtime_error("Cannot read scene");
    p.asset = p.loader->createAsset(bytes.data(), static_cast<uint32_t>(bytes.size()));
    if (!p.asset)
        throw std::runtime_error("Invalid glTF scene: " + path.string());
    const auto source_path = path.string();
    gltfio::ResourceConfiguration config{};
    config.gltfPath = source_path.c_str();
    config.engine = &e;
    config.normalizeSkinningWeights = true;
    gltfio::ResourceLoader resources(config);
    for (size_t i = 0; i < p.asset->getResourceUriCount(); ++i) {
        const char* uri = p.asset->getResourceUris()[i];
        std::ifstream resource(path.parent_path() / uri, std::ios::binary);
        if (!resource)
            throw std::runtime_error(std::string("Missing scene resource: ") + uri);
        auto data = std::make_unique<std::vector<uint8_t>>(std::istreambuf_iterator<char>(resource),
                                                           std::istreambuf_iterator<char>{});
        auto* owned = data.release();
        resources.addResourceData(
            uri, gltfio::ResourceLoader::BufferDescriptor(
                     owned->data(), owned->size(),
                     [](void*, size_t, void* context) { delete static_cast<std::vector<uint8_t>*>(context); },
                     owned));
    }
    p.textures.reset(gltfio::createStbProvider(&e));
    resources.addTextureProvider("image/png", p.textures.get());
    resources.addTextureProvider("image/jpeg", p.textures.get());
    if (!resources.loadResources(p.asset))
        throw std::runtime_error("Failed to load greenhouse resources");
    auto& renderables = e.getRenderableManager();
    auto& transforms = e.getTransformManager();
    for (size_t i = 0; i < p.asset->getEntityCount(); ++i) {
        const auto entity = p.asset->getEntities()[i];
        const char* name = p.asset->getName(entity);
        if (name && std::string_view(name).starts_with("Glazing")) {
            auto instance = renderables.getInstance(entity);
            if (instance)
                renderables.setCastShadows(instance, false);
        }
    }
    p.scene->addEntities(p.asset->getEntities(), p.asset->getEntityCount());
    p.asset->releaseSourceData();
    p.gardens = p.load(path.parent_path() / "gardens.glb", false);
    p.fauna = std::make_unique<fauna_meshes>(
        e, *p.scene, *p.loader, p.load(path.parent_path() / "fauna.glb", false), path.parent_path());
    p.glass = std::make_unique<vessel_glass>(e, path.parent_path() / "glass.filamat");
    for (size_t i = 0; i < p.gardens->getEntityCount(); ++i) {
        auto entity = p.gardens->getEntities()[i];
        auto* name = p.gardens->getName(entity);
        if (!name)
            continue;
        int a, b, c;
        if (std::string_view(name) == "Gardener boot left" || std::string_view(name) == "Gardener boot right")
            p.boots[std::string_view(name).ends_with("left") ? 0 : 1] = {
                entity, transforms.getTransform(transforms.getInstance(entity))};
        if (std::sscanf(name, "Placement %d", &a) == 1 && a >= 0 && a < 10)
            p.markers[a] = entity;
        if (std::sscanf(name, "Garden %d", &a) == 1 && a >= 0 && a < 10)
            p.jars[a] = entity;
        if (std::sscanf(name, "Plant %d %d", &a, &b) == 2 && a >= 0 && a < 10 && b >= 0 && b < 320)
            p.plants[a][b] = {entity, transforms.getTransform(transforms.getInstance(entity))};
        if (std::sscanf(name, "Foliage %d %d %d", &a, &b, &c) == 3 && a >= 0 && a < 10 && b >= 0 && b < 320 &&
            c >= 0 && c < 3)
            p.foliage[a][b][c] = {entity, transforms.getTransform(transforms.getInstance(entity))};
        if (std::sscanf(name, "Preview %d", &a) == 1 && a >= 0 && a < 6)
            p.previews[a] = {entity, transforms.getTransform(transforms.getInstance(entity))};
        if (std::sscanf(name, "Vessel %d %d", &a, &b) == 2 && a >= 0 && a < 10 && b >= 0 && b < 4) {
            p.vessels[a][b] = {entity, transforms.getTransform(transforms.getInstance(entity))};
            auto r = renderables.getInstance(entity);
            if (r) {
                renderables.setCastShadows(r, false);
                if (b == 0 || b == 3)
                    renderables.setMaterialInstanceAt(r, 0, p.glass->instance());
            }
        }
    }
    for (unsigned i = 0; i < 10; ++i) {
        for (auto& variants : p.foliage[i])
            for (auto& n : variants)
                p.collect(n, n.entity);
        for (auto& n : p.vessels[i])
            p.collect(n, n.entity);
        p.scene->addEntity(p.markers[i]);
    }
    for (auto& n : p.previews) {
        p.collect(n, n.entity);
        for (auto entity : n.renderables)
            renderables.setCastShadows(renderables.getInstance(entity), false);
    }
    p.substrate = std::make_unique<substrate_meshes>(
        e, *p.scene, *renderables.getMaterialInstanceAt(renderables.getInstance(p.vessels[0][1].entity), 0),
        path.parent_path() / "water.filamat");
    for (unsigned kind = 0; kind < p.plant_radius.size(); ++kind) {
        for (auto entity : p.foliage[0][0][kind].renderables) {
            const auto bounds = renderables.getAxisAlignedBoundingBox(renderables.getInstance(entity));
            const auto transform = transforms.getWorldTransform(transforms.getInstance(entity));
            for (int x : {-1, 1})
                for (int y : {-1, 1})
                    for (int z : {-1, 1}) {
                        const auto corner =
                            transform *
                            math::float4(bounds.center +
                                             bounds.halfExtent * math::float3{float(x), float(y), float(z)},
                                         1);
                        p.plant_radius[kind] = std::max(p.plant_radius[kind], std::hypot(corner.x, corner.z));
                    }
        }
        if (!std::isfinite(p.plant_radius[kind]) || p.plant_radius[kind] <= 0)
            throw std::runtime_error("Imported plant canopy has invalid bounds");
    }
    for (auto& n : p.boots) {
        p.collect(n, n.entity);
        p.show(n, true);
    }
    p.character = p.load(path.parent_path() / "gardener.glb");
    for (size_t i = 0; i < p.character->getEntityCount(); ++i) {
        auto entity = p.character->getEntities()[i];
        auto* name = p.character->getName(entity);
        auto t = transforms.getInstance(entity);
        if (name && t)
            p.joints[name] = {entity, transforms.getTransform(t)};
        if (name &&
            (std::string_view(name) == "First person hidden head" || std::string_view(name) == "Eyebrows" ||
             std::string_view(name) == "Eyes" || std::string_view(name) == "Hair_Buns")) {
            auto r = renderables.getInstance(entity);
            if (r)
                renderables.setLayerMask(r, 0xff, 0x02);
        }
    }
    auto* animator = p.character->getInstance()->getAnimator();
    for (size_t i = 0; i < animator->getAnimationCount(); ++i)
        p.clips[animator->getAnimationName(i)] = i;
    for (auto name : {"Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop", "Crouch_Idle_Loop",
                      "Crouch_Fwd_Loop", "Jump_Loop"})
        if (!p.clips.contains(name))
            throw std::runtime_error(std::string("Missing gardener animation: ") + name);
    for (unsigned i = 0; i < 2; ++i) {
        auto foot =
            transforms.getWorldTransform(transforms.getInstance(p.joints.at(i ? "foot_r" : "foot_l").entity));
        p.boot_attachments[i] =
            inverse(foot) * math::mat4f::translation(math::float3{foot[3].x, 0, foot[3].z}) * p.boots[i].bind;
    }
    p.hud = std::make_unique<sengine::native_hud>(e, path.parent_path() / "hud.filamat",
                                                  path.parent_path().parent_path() / "fonts/Body.ttf");
}
filament_scene::~filament_scene() = default;

sengine::native_hud& filament_scene::hud() {
    return *impl_->hud;
}
void filament_scene::configure(int detail, bool occlusion, bool soft_focus, bool inspecting,
                               float focus_distance) {
    auto& p = *impl_;
    p.camera->setFocusDistance(std::max(.08f, focus_distance));
    auto settings = std::tuple(detail, occlusion, soft_focus, inspecting);
    if (p.settings == settings)
        return;
    p.settings = settings;
    p.near_plane = inspecting ? .006f : .045f;
    auto taa = p.view->getTemporalAntiAliasingOptions();
    taa.enabled = detail > 0;
    p.view->setTemporalAntiAliasingOptions(taa);
    auto ao = p.view->getAmbientOcclusionOptions();
    ao.enabled = occlusion && detail > 0;
    ao.radius = inspecting ? .035f : .25f;
    ao.power = .85f;
    ao.resolution = detail == 2 ? 1.f : .5f;
    ao.quality = detail == 2 ? QualityLevel::HIGH : QualityLevel::MEDIUM;
    p.view->setAmbientOcclusionOptions(ao);
    p.view->setShadowType(detail == 2 ? ShadowType::PCSS : ShadowType::PCF);
    auto& lights = p.engine->getLightManager();
    auto sun = lights.getInstance(p.sun);
    auto shadows = lights.getShadowOptions(sun);
    const unsigned size = detail == 0 ? 1024 : 2048;
    if (shadows.mapSize != size) {
        shadows.mapSize = size;
        lights.setShadowOptions(sun, shadows);
    }
    auto dof = p.view->getDepthOfFieldOptions();
    dof.enabled = soft_focus && inspecting && detail > 0;
    dof.cocScale = .3f;
    dof.maxForegroundCOC = 3;
    dof.maxBackgroundCOC = 12;
    p.view->setDepthOfFieldOptions(dof);
}
void filament_scene::present(const greenhouse& game, const sengine::camera_pose& player, double seconds,
                             std::optional<tending_preview> preview) {
    auto& p = *impl_;
    p.outside_body = game.mode() == greenhouse_mode::editor ||
                     (game.mode() == greenhouse_mode::enter_editor && game.progress() > .8f) ||
                     (game.mode() == greenhouse_mode::leave_editor && game.progress() < .2f);
    p.player_head = player.eye;
    auto& tm = p.engine->getTransformManager();
    auto point = [](sengine::point a) { return math::float3{a.x, a.y, a.z}; };
    const math::float3 forward{std::sin(player.yaw), 0, -std::cos(player.yaw)},
        right{std::cos(player.yaw), 0, std::sin(player.yaw)};
    const auto* active = game.active();
    const auto carry = compute_carry_pose(player, active ? active->world.design() : garden_design{});
    const auto held = point(carry.base);
    const float hold = game.carry_blend();
    auto ease = [](float x) {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3 - 2 * x);
    };
    float t = game.progress(), support = hold;
    if (game.mode() == greenhouse_mode::lifting) {
        support = ease(t / .28f);
    }
    if (game.mode() == greenhouse_mode::lowering) {
        support = 1 - ease((t - .72f) / .28f);
    }
    math::float3 grip = held;
    tm.openLocalTransformTransaction();
    for (size_t i = 0; i < 10; ++i) {
        const greenhouse_garden* g = nullptr;
        for (const auto& candidate : game.gardens())
            if (candidate.place == int(i)) {
                g = &candidate;
                break;
            }
        auto mark = point(greenhouse::spots()[i].position);
        mark.y += .009f;
        tm.setTransform(tm.getInstance(p.markers[i]),
                        math::mat4f::translation(game.mode() == greenhouse_mode::carry && (!g || g == active)
                                                     ? mark
                                                     : math::float3{0, -100, 0}));
        for (auto& n : p.vessels[i])
            p.show(n, g != nullptr);
        auto root = tm.getInstance(p.jars[i]);
        if (!g) {
            p.substrate->update(i, nullptr, {});
            for (unsigned j = 0; j < p.previous_plants[i]; ++j)
                for (auto& n : p.foliage[i][j])
                    p.show(n, false);
            p.previous_plants[i] = 0;
            p.animal_motion[i] = {};
            continue;
        }
        float radius = float(g->world.design().radius()), height = float(g->world.design().height());
        float soil_top = float(g->world.design().soil_depth + g->world.design().drainage_depth) / height;
        float drain = float(g->world.design().drainage_depth) / height,
              soil = float(g->world.design().soil_depth) / height;
        tm.setTransform(tm.getInstance(p.vessels[i][1].entity),
                        math::mat4f::translation(math::float3{0, drain - .08f * soil / .155f, 0}) *
                            math::mat4f::scaling(math::float3{1, soil / .155f, 1}));
        tm.setTransform(tm.getInstance(p.vessels[i][2].entity),
                        math::mat4f::scaling(math::float3{1, drain / .08f, 1}));
        auto location = point(greenhouse::spots()[i].position);
        if (g == active && game.holding()) {
            auto anchor = location;
            if (game.mode() == greenhouse_mode::lowering)
                anchor = point(greenhouse::spots()[game.destination()].position);
            location = anchor * (1 - hold) + held * hold;
            grip = location;
        }
        auto transform = math::mat4f::translation(location) *
                         math::mat4f::rotation(g->rotation, math::float3{0, 1, 0}) *
                         math::mat4f::scaling(math::float3{radius, height, radius});
        if (transform != p.jar_transforms[i]) {
            tm.setTransform(root, transform);
            p.jar_transforms[i] = transform;
        }
        p.substrate->update(i, &g->world, transform);
        p.animal_motion[i].update(g->world, g->id);
        for (size_t j = 0; j < std::max<size_t>(p.previous_plants[i], g->world.plants().size()); ++j) {
            auto& n = p.plants[i][j];
            auto inst = tm.getInstance(n.entity);
            if (j >= g->world.plants().size()) {
                for (auto& f : p.foliage[i][j])
                    p.show(f, false);
                continue;
            }
            const auto& plant = g->world.plants()[j];
            for (unsigned k = 0; k < 3; ++k) {
                auto& f = p.foliage[i][j][k];
                p.show(f, k == unsigned(plant.kind));
            }
            float growth = std::clamp(world_state::growth_scale(plant), .15f, 1.f);
            const float x = plant.position.x / world_state::radius * .80f;
            const float z = plant.position.y / world_state::radius * .80f;
            const float clearance = std::max(.015f, .88f - std::hypot(x, z)) * radius;
            const float desired_height = (1 - soil_top) * height * (.30f + .42f * growth);
            const float plant_height =
                std::min(desired_height, clearance / p.plant_radius[unsigned(plant.kind)]);
            tm.setTransform(
                inst, math::mat4f::translation(math::float3{x, soil_top + .004f, z}) *
                          math::mat4f::rotation(float(plant.shape % 628) * .01f, math::float3{0, 1, 0}) *
                          math::mat4f::scaling(math::float3{plant_height / radius, plant_height / height,
                                                            plant_height / radius}) *
                          n.bind);
        }
        for (const auto& bug : g->world.creatures()) {
            const float size = creature_length(bug);
            const auto pose =
                transform *
                math::mat4f::translation(math::float3{
                    bug.position.x / world_state::radius * .8f,
                    soil_top + (presentation::bed_offset(g->world, bug.position) + .001f) / height,
                    bug.position.y / world_state::radius * .8f}) *
                math::mat4f::scaling(math::float3{size / radius, size / height, size / radius}) *
                math::mat4f::rotation(-bug.heading, math::float3{0, 1, 0});
            p.fauna->place(bug.species, pose, p.animal_motion[i].weights(bug.id));
        }
    }
    p.fauna->finish();
    for (unsigned kind = 0; kind < p.previews.size(); ++kind) {
        auto& n = p.previews[kind];
        const bool visible = preview && active && active->place >= 0 &&
                             game.mode() == greenhouse_mode::editor && int(preview->tool) == int(kind) + 1;
        p.show(n, visible);
        if (!visible)
            continue;
        const auto& design = active->world.design();
        const float radius = float(design.radius()), height = float(design.height());
        const float soil_top = float(design.soil_depth + design.drainage_depth) / height;
        const float x = preview->position.x / world_state::radius * .8f;
        const float z = preview->position.y / world_state::radius * .8f;
        float size;
        if (kind < 3) {
            plant_state plant{};
            plant.kind = plant_kind(kind);
            plant.biomass = .45f;
            const float growth = std::clamp(world_state::growth_scale(plant), .15f, 1.f);
            const float clearance = std::max(.015f, .88f - std::hypot(x, z)) * radius;
            size =
                std::min((1 - soil_top) * height * (.30f + .42f * growth), clearance / p.plant_radius[kind]);
        } else {
            creature_state creature{};
            creature.species = species_kind(kind - 3);
            creature.age = maturity_age(creature.species);
            size = creature_length(creature);
        }
        tm.setTransform(
            tm.getInstance(n.entity),
            p.jar_transforms[active->place] *
                math::mat4f::translation(math::float3{
                    x,
                    soil_top +
                        (kind < 3
                             ? presentation::surface_offset(active->world, preview->position) / height + .004f
                             : (presentation::bed_offset(active->world, preview->position) + .001f) / height),
                    z}) *
                math::mat4f::scaling(math::float3{size / radius, size / height, size / radius}) * n.bind);
    }
    tm.commitLocalTransformTransaction();
    p.previous_plants.fill(0);
    for (const auto& g : game.gardens())
        if (g.place >= 0) {
            p.previous_plants[g.place] = g.world.plants().size();
        }

    for (auto& [name, j] : p.joints)
        tm.setTransform(tm.getInstance(j.entity), j.bind);
    auto* animator = p.character->getInstance()->getAnimator();
    const bool crouched = player.eye.y - player.feet.y < 1.40f;
    const char* clip = !player.grounded      ? "Jump_Loop"
                       : crouched            ? (player.speed > .15f ? "Crouch_Fwd_Loop" : "Crouch_Idle_Loop")
                       : player.speed > 4.f  ? "Sprint_Loop"
                       : player.speed > 3.1f ? "Jog_Fwd_Loop"
                       : player.speed > .15f ? "Walk_Loop"
                                             : "Idle_Loop";
    auto selected = p.clips.at(clip);
    if (!p.clip_started || selected != p.active_clip) {
        p.previous_clip = p.active_clip;
        p.previous_clip_time = p.clip_time;
        p.active_clip = selected;
        p.blend_time = p.clip_started ? 0.f : 1.f;
        p.clip_started = true;
        p.clip_time = 0;
    }
    const float duration = animator->getAnimationDuration(p.active_clip);
    p.clip_time = player.grounded && player.speed > .15f
                      ? std::fmod(player.stride / (2 * 3.14159265f), 1.f) * duration
                      : std::fmod(p.clip_time + float(seconds), duration);
    animator->applyAnimation(p.active_clip, p.clip_time);
    p.blend_time += float(seconds);
    if (p.blend_time < .18f)
        animator->applyCrossFade(p.previous_clip, p.previous_clip_time, ease(p.blend_time / .18f));
    auto body_root = tm.getInstance(p.character->getRoot());
    auto body_position = point(player.feet);
    const auto body_rotation = math::mat4f::rotation(3.14159265f - player.yaw, math::float3{0, 1, 0});
    tm.setTransform(body_root, math::mat4f::translation(body_position) * body_rotation);
    auto entity = [&](const std::string& name) { return p.joints.at(name).entity; };
    auto world = [&](const std::string& name) { return tm.getWorldTransform(tm.getInstance(entity(name))); };
    auto position = [&](const std::string& name) { return world(name)[3].xyz; };
    auto rotate_world = [&](const std::string& name, math::float3 axis, float radians) {
        auto inst = tm.getInstance(entity(name));
        auto w = tm.getWorldTransform(inst);
        auto pos = w[3].xyz;
        auto parent = tm.getParent(inst);
        auto pw = parent ? tm.getWorldTransform(tm.getInstance(parent)) : math::mat4f{};
        tm.setTransform(inst, inverse(pw) * math::mat4f::translation(pos) *
                                  math::mat4f::rotation(radians, axis) * math::mat4f::translation(-pos) * w);
    };
    auto aim = [&](const std::string& name, const std::string& child, math::float3 target) {
        auto a = position(name);
        auto from = normalize(position(child) - a), to = normalize(target - a);
        auto axis = cross(from, to);
        float l = length(axis);
        if (l > .00001f)
            rotate_world(name, axis / l, std::acos(std::clamp(dot(from, to), -1.f, 1.f)));
    };
    auto ik = [&](const std::string& upper, const std::string& lower, const std::string& end,
                  math::float3 target, math::float3 pole) {
        auto a = position(upper);
        float l1 = length(position(lower) - a), l2 = length(position(end) - position(lower));
        auto delta = target - a;
        float len = std::clamp(length(delta), std::abs(l1 - l2) + .001f, l1 + l2 - .001f);
        auto dir = normalize(delta);
        auto side = normalize(pole - a - dir * dot(pole - a, dir));
        float along = (l1 * l1 - l2 * l2 + len * len) / (2 * len);
        auto middle = a + dir * along + side * std::sqrt(std::max(0.f, l1 * l1 - along * along));
        aim(upper, lower, middle);
        aim(lower, end, a + dir * len);
    };
    if (player.grounded) {
        const float lowest_ankle = std::min(position("foot_l").y, position("foot_r").y);
        body_position.y += player.feet.y + .07f - lowest_ankle;
        tm.setTransform(body_root, math::mat4f::translation(body_position) * body_rotation);
    }

    std::array<math::float3, 2> authored_hands{position("hand_l"), position("hand_r")};
    if (support > 0) {
        for (auto& [name, joint] : p.joints) {
            if (!(name.starts_with("upperarm_") || name.starts_with("lowerarm_") ||
                  name.starts_with("hand_") || name.starts_with("index_") || name.starts_with("middle_") ||
                  name.starts_with("ring_") || name.starts_with("pinky_") || name.starts_with("thumb_")))
                continue;
            auto instance = tm.getInstance(joint.entity);
            auto local = tm.getTransform(instance);
            auto rotation = slerp(local.toQuaternion(), joint.bind.toQuaternion(), support);
            auto blended = math::mat4f(rotation);
            blended[3] = local[3];
            tm.setTransform(instance, blended);
        }
    }
    float reach_lean =
        (std::clamp((dot(grip - body_position, forward) - .68f) * 1.8f, 0.f, .6f) * (1 - hold) +
         .08f * hold) *
        support;
    if (reach_lean > 0)
        rotate_world("spine_01", right, -reach_lean);
    for (auto side : {std::string("l"), std::string("r")}) {

        float sign = side == "l" ? -1.f : 1.f;
        const unsigned boot_index = side == "l" ? 0 : 1;
        auto& boot = p.boots[boot_index];
        tm.setTransform(tm.getInstance(boot.entity), world("foot_" + side) * p.boot_attachments[boot_index]);
        if (support <= 0)
            continue;
        auto shoulder = position("upperarm_" + side);
        auto rest_hand = authored_hands[boot_index];
        auto held_hand = point(carry.wrists[boot_index]) + grip - held;
        auto hand = rest_hand * (1 - support) + held_hand * support;
        ik("upperarm_" + side, "lowerarm_" + side, "hand_" + side, hand,
           shoulder + right * sign * .30f + math::float3{0, -.45f, 0} - forward * .05f);
        auto toward = support > .01f ? forward * .06f - right * sign * .09f : math::float3{0, -.12f, 0};
        aim("hand_" + side, "middle_01_" + side, position("hand_" + side) + toward);
        const auto wrist = position("hand_" + side);
        const auto finger_axis = normalize(position("middle_01_" + side) - wrist);
        auto palm =
            normalize(cross(position("index_01_" + side) - wrist, position("pinky_01_" + side) - wrist)) *
            sign;
        palm = normalize(palm - finger_axis * dot(palm, finger_axis));
        auto up = normalize(math::float3{0, 1, 0} - finger_axis * finger_axis.y);
        const float roll = std::atan2(dot(finger_axis, cross(palm, up)), dot(palm, up));
        rotate_world("hand_" + side, finger_axis, roll * support);
        for (auto f : {"index", "middle", "ring", "pinky"})
            for (auto segment : {"01", "02", "03"}) {
                std::string name = std::string(f) + "_" + segment + "_" + side;
                auto inst = tm.getInstance(entity(name));
                auto local = tm.getTransform(inst);
                tm.setTransform(inst, local * math::mat4f::rotation(support * .08f, math::float3{1, 0, 0}));
            }
    }
    animator->updateBoneMatrices();
}

bool filament_scene::frame(const sengine::camera_pose& camera, unsigned width, unsigned height,
                           const std::filesystem::path& capture, bool capture_interface) {
    if (!width || !height)
        return false;
    auto& p = *impl_;
    const auto eye = camera.eye;
    float head_distance =
        std::hypot(eye.x - p.player_head.x, eye.y - p.player_head.y, eye.z - p.player_head.z);
    p.view->setVisibleLayers(0xff, p.outside_body && head_distance > .4f ? 0x03 : 0x01);
    return p.graphics.frame(camera, width, height, p.near_plane, 330.f, p.hud.get(), capture,
                            capture_interface);
}
}
