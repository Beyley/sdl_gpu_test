const std = @import("std");

fn compileShader(b: *std.Build, optimize: std.builtin.OptimizeMode, input: std.Build.LazyPath) std.Build.LazyPath {
    const command = b.addSystemCommand(&.{
        "glslc",
        "--target-env=vulkan1.0",
        "-x",
        "glsl",
        "-mfmt=bin",
        "-Werror",
        "-c",
    });

    switch (optimize) {
        .Debug => {
            command.addArg("-g");
            command.addArg("-O0");
        },
        .ReleaseFast, .ReleaseSafe => {
            command.addArg("-O");
        },
        .ReleaseSmall => {
            command.addArg("-Os");
        },
    }

    command.addFileArg(input);

    command.addArg("-o");
    return command.addOutputFileArg(input.getDisplayName());
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const spirv_cross_dep = b.dependency("spirv_cross", .{
        .target = target,
        .optimize = optimize,
        .spv_cross_cpp = false,
        .spv_cross_reflect = false,
        .spv_cross_build_shared = false,
    });

    const files = b.addStaticLibrary(.{
        .name = "files",
        .root_source_file = b.path("src/embed.zig"),
        .target = target,
        .optimize = optimize,
    });

    const vert = compileShader(b, optimize, b.path("src/shaders/main.vert"));
    const frag = compileShader(b, optimize, b.path("src/shaders/main.frag"));

    vert.addStepDependencies(&files.step);
    frag.addStepDependencies(&files.step);

    files.root_module.addAnonymousImport("main.vert", .{ .root_source_file = vert });
    files.root_module.addAnonymousImport("main.frag", .{ .root_source_file = frag });
    files.root_module.addAnonymousImport("texture.png", .{ .root_source_file = b.path("src/texture.png") });

    const exe = b.addExecutable(.{
        .name = "sdl_gpu_test",
        .target = target,
        .optimize = optimize,
    });

    exe.linkLibrary(spirv_cross_dep.artifact("spirv-cross-c"));
    exe.root_module.addCMacro("SDL_GPU_SHADERCROSS_SPIRV", "1");
    exe.root_module.addCMacro("SDL_GPU_SHADERCROSS_STATIC", "1");

    exe.linkLibrary(files);

    exe.addIncludePath(b.path("SDL_gpu_shadercross/"));
    exe.addIncludePath(b.path("stb/"));
    exe.linkLibC();
    exe.linkSystemLibrary("sdl3");

    const standard_flags: []const []const u8 = &.{
        "-std=gnu23",
    };

    exe.addCSourceFiles(.{
        .files = &.{
            "main.c",
            "gfx.c",
        },
        .flags = standard_flags ++ .{
            "-Weverything",
            "-Wpedantic",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-pre-c23-compat",
            "-Wno-declaration-after-statement",
            "-Wno-covered-switch-default",
            "-Wno-c++98-compat",
            "-Wno-documentation-unknown-command",
        },
        .root = b.path("src"),
    });

    exe.addCSourceFiles(.{
        .files = &.{
            "SDL_gpu_shadercross.c",
            "stb_image.c",
        },
        .flags = standard_flags,
        .root = b.path("src"),
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
