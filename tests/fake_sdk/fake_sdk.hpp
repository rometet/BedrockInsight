#pragma once
// Failure-injection harness for our adapter only. This is not a Bedrock ABI
// substitute and never participates in the native DLL build.
#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <vector>

struct BlockPos { int x = 0, y = 0, z = 0; };
struct TestUuid { unsigned long long a = 1, b = 2; };
class Level {
public:
    int online = 0;
    int $getActivePlayerCount() const { return online; }
};
class Actor;
template<class T> struct TestOptionalRef {
    T* value;
    explicit operator bool() const {return value!=nullptr;}
    T* as_ptr() const {return value;}
    T* operator->() const {return value;}
};
class WeakEntityRef {
public:
    Actor* actor;
    template<class T> TestOptionalRef<T> tryUnwrap() const;
};
struct TestEntityContext {
    Actor* actor;
    WeakEntityRef getWeakRef() const {return {actor};}
};
class Actor {
public:
    bool sprinting=false,sneaking=false,valid=true;
    Level* level=nullptr;
    bool isSprinting() const {return sprinting;}
    bool isSneaking() const {return sneaking;}
    Level& getLevel() const {return *level;}
    TestEntityContext getEntityContext() {return {this};}
};
template<class T> TestOptionalRef<T> WeakEntityRef::tryUnwrap() const {
    return {actor && actor->valid ? static_cast<T*>(actor) : nullptr};
}
class Player : public Actor {
public:
    TestUuid uuid;
    std::string name = "Test";
    TestUuid const& getUuid() const { return uuid; }
    std::string getRealName() const { return name; }
    int getDimensionId() const { return 0; }
};
using ServerPlayer = Player;
namespace ll {
struct TestLoaderVersion { unsigned major=26, minor=60, patch=0; bool preRelease=false; };
inline TestLoaderVersion test_loader_version;
inline bool test_is_client=false;
inline bool test_version_throws=false;
inline TestLoaderVersion getLoaderVersion() {
    if (test_version_throws) throw std::runtime_error("injected loader version error");
    return test_loader_version;
}
inline bool isClient() { return test_is_client; }
}
namespace ll::mod {
struct TestLogger {
    void info(char const*) {}
    void error(char const*) {}
};
class NativeMod {
public:
    static std::shared_ptr<NativeMod> current() {
        static auto instance = std::make_shared<NativeMod>();
        return instance;
    }
    std::filesystem::path data_dir;
    std::filesystem::path const& getDataDir() const { return data_dir; }
    TestLogger& getLogger() { return logger; }
private:
    TestLogger logger;
};
}
namespace ll::event {
struct ServerLevelTickEvent {
    Level& value;
    Level& level() { return value; }
};
struct PlayerJoinEvent {
    Player& player;
    bool cancelled = false;
    Player& self() { return player; }
    bool isCancelled() const { return cancelled; }
};
struct PlayerDieEvent {
    Player& player;
    Player& self() { return player; }
};
struct PlayerUseItemEvent {
    Player& player;
    bool cancelled = false;
    Player& self() { return player; }
    bool isCancelled() const { return cancelled; }
};
struct PlayerSprintingEvent {Player& player;Player& self(){return player;}};
struct PlayerSprintedEvent {Player& player;Player& self(){return player;}};
struct PlayerSneakingEvent {
    Player& player;bool cancelled=false;
    Player& self(){return player;}bool isCancelled() const{return cancelled;}
};
struct PlayerSneakedEvent {
    Player& player;bool cancelled=false;
    Player& self(){return player;}bool isCancelled() const{return cancelled;}
};
struct PlayerDisconnectEvent {
    Player& player;
    Player& self() { return player; }
};
struct PlayerDestroyBlockEvent {
    Player& player;
    BlockPos position;
    bool cancelled = false;
    Player& self() { return player; }
    BlockPos const& pos() { return position; }
    bool isCancelled() const { return cancelled; }
};
struct PlayerPlacedBlockEvent {
    Player& player;
    BlockPos position;
    Player& self() { return player; }
    BlockPos const& pos() { return position; }
};
enum class EventPriority { Lowest };
class ListenerBase { public: virtual ~ListenerBase() = default; };
using ListenerPtr = std::shared_ptr<ListenerBase>;
template<class Event> class Listener : public ListenerBase {
public:
    std::function<void(Event&)> callback;
    static std::shared_ptr<Listener> create(std::function<void(Event&)> fn, EventPriority) {
        auto result = std::make_shared<Listener>();
        result->callback = std::move(fn);
        return result;
    }
};
class EventBus {
public:
    struct Registration { std::type_index type; ListenerPtr listener; };
    std::vector<Registration> registrations;
    int add_calls = 0;
    int fail_add = 0;
    bool fail_remove = false;
    static EventBus& getInstance() { static EventBus bus; return bus; }
    template<class Event> bool addListener(std::shared_ptr<Listener<Event>> const& listener) {
        if (++add_calls == fail_add) throw std::runtime_error("injected registration failure");
        registrations.push_back({typeid(Event), listener});
        return true;
    }
    bool hasListener(ListenerPtr const& listener) const {
        return std::any_of(registrations.begin(), registrations.end(),
            [&](auto const& value) { return value.listener == listener; });
    }
    bool removeListener(ListenerPtr const& listener) {
        if (fail_remove) return false;
        std::erase_if(registrations, [&](auto const& value) { return value.listener == listener; });
        return true;
    }
    template<class Event> void publish(Event& event) {
        auto snapshot = registrations;
        for (auto const& value : snapshot) if (value.type == typeid(Event)) {
            std::static_pointer_cast<Listener<Event>>(value.listener)->callback(event);
        }
    }
};
}
