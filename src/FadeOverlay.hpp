#pragma once

#include <memory>

namespace md3 {

class FadeOverlay {
public:
    FadeOverlay();
    ~FadeOverlay();

    FadeOverlay(const FadeOverlay&) = delete;
    FadeOverlay& operator=(const FadeOverlay&) = delete;

    void render(float opacity) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace md3
