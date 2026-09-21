#pragma once
#include <backend/BufferDescriptor.h>
#include <vector>

namespace terrarium::render {
template <class element> filament::backend::BufferDescriptor upload(std::vector<element> values) {
    auto* data = new std::vector<element>(std::move(values));
    return {data->data(), data->size() * sizeof(element),
            [](void*, size_t, void* p) { delete static_cast<std::vector<element>*>(p); }, data};
}
}
