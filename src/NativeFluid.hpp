#pragma once
#include <memory>
namespace md3 {
// One precompiled procedural shader; look changes only update uniforms.
class NativeFluid {
public:
    NativeFluid();
    ~NativeFluid();
    NativeFluid(const NativeFluid&) = delete;
    NativeFluid& operator=(const NativeFluid&) = delete;
    void render(float time, float look, float energy, float aspect) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
