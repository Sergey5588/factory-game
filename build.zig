const std = @import("std");  
  
pub fn build(b: *std.Build) void {  
    const target = b.standardTargetOptions(.{});  
    const optimize = b.standardOptimizeOption(.{});  
  
    // Build SDL3 and SDL3_ttf from source via the dependency packages  
    const sdl_dep = b.dependency("sdl", .{  
        .target = target,  
        .optimize = optimize,  
    });  
    const flecs_dep = b.dependency("zflecs", .{
        .target = target,
        .optimize = optimize,
    });
  
    const exe = b.addExecutable(.{  
        .name = "factory",  
        .root_module = b.createModule(.{  
            .root_source_file = null,  
            .target = target,  
            .optimize = optimize,  
            .link_libc = true,  
        }),  
    });  
    //add C sources
    exe.root_module.addCSourceFiles(.{  
        .files = &.{
            "src/main.c",
            "src/systems.c",
            "src/input.c",
        },  
        .flags = &.{  
            "-std=c23",             "-Wall",  
            "-Wextra",              "-Wno-unused-variable",  
            "-Wno-sign-compare",    "-Wno-missing-field-initializers",  
            "-Wno-unused-parameter",  
        },  
    });  
  
    exe.root_module.addIncludePath(b.path("vendor"));  
    exe.root_module.addIncludePath(flecs_dep.path("libs/flecs"));
    // Link the built-from-source SDL3
    exe.root_module.linkLibrary(sdl_dep.artifact("SDL3"));  
    // flecs
    exe.root_module.linkLibrary(flecs_dep.artifact("flecs"));  
    exe.subsystem = .Windows;
    b.installArtifact(exe);  
  
    // Copy resources  
    b.installDirectory(.{  
        .source_dir = b.path("resources"),  
        .install_dir = .bin,  
        .install_subdir = "resources",  
    });  
  
    // Run step  
    const run_cmd = b.addRunArtifact(exe);  
    run_cmd.step.dependOn(b.getInstallStep());  
    if (b.args) |args| run_cmd.addArgs(args);  
    const run_step = b.step("run", "Run the application");  
    run_step.dependOn(&run_cmd.step);  
}
