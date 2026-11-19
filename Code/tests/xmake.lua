
target("TPTests")
    set_kind("binary")
    set_group("Tests")
    add_includedirs(
        ".", "../encoding", "../client")
    add_headerfiles("**.h")
    add_files("*.cpp")
    -- Has no game dependencies, so the client's remote playback timing can be tested here
    add_files("../client/Systems/PlaybackClock.cpp")
    add_files("../client/Services/Generic/RunnerService.cpp")
    add_deps("SkyrimEncoding")
    add_packages(
        "tiltedcore",
        "hopscotch-map",
        "catch2",
        "entt",
        "mimalloc",
        "glm")

if is_plat("windows") then
    target("HostingIntegration")
        set_default(false)
        set_kind("binary")
        set_group("Tests")
        add_includedirs("../client", "../encoding")
        add_files("integration/hosting.cpp", "../client/Services/Generic/HostSession.cpp")
        add_deps("SkyrimEncoding", "CommonLib", "TiltedConnect")
        add_packages("miniupnpc", "tiltedcore", "hopscotch-map", "glm", "gamenetworkingsockets", "snappy")
end
