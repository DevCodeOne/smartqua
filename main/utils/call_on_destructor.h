#pragma once

template<typename Resource, typename Callable>
class FreeableRessource {
public:
    FreeableRessource() = default;
    FreeableRessource(Resource resource, Callable callable)
        : mResource(resource), mCallable(callable) {
    }

    FreeableRessource(FreeableRessource &&other) = default;

    FreeableRessource &operator=(FreeableRessource &&other) = default;

    FreeableRessource(const FreeableRessource &other) = delete;

    FreeableRessource &operator=(const FreeableRessource &other) = delete;

    const Resource &resource() const { return mResource; }

    Resource &resource() { return mResource; }

    ~FreeableRessource() {
        if (mCallable && mResource) {
            (*mCallable)(*mResource);
        }
    }

private:
    std::optional<Resource> mResource{std::nullopt};
    std::optional<Callable> mCallable{std::nullopt};
};

template<typename Resource, typename Callable>
auto MakeFreeableResource(Resource resource, Callable callable) {
    return FreeableRessource(resource, std::move(callable));
}
