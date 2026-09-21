#include "render/vessel_glass.hpp"
#include <filament/Engine.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace terrarium::render {
struct vessel_glass::impl {
    filament::Engine& engine;
    filament::Material* material{};
    filament::MaterialInstance* instance{};
    explicit impl(filament::Engine& e) : engine(e) {}
    ~impl() {
        engine.destroy(instance);
        engine.destroy(material);
    }
};
vessel_glass::vessel_glass(filament::Engine& engine, const std::filesystem::path& path)
    : impl_(std::make_unique<impl>(engine)) {
    std::ifstream input(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
    if (bytes.empty())
        throw std::runtime_error("Glass material unavailable");
    impl_->material = filament::Material::Builder().package(bytes.data(), bytes.size()).build(engine);
    if (!impl_->material)
        throw std::runtime_error("Glass material invalid");
    impl_->instance = impl_->material->createInstance();
}
vessel_glass::~vessel_glass() = default;
filament::MaterialInstance* vessel_glass::instance() const {
    return impl_->instance;
}
}
