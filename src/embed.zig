const std = @import("std");

fn exportFile(comptime module_name: []const u8, comptime export_name: []const u8) void {
    const contents = @embedFile(module_name);

    const Fns = struct {
        pub fn getSize() callconv(.C) c_uint {
            return @intCast(contents.len);
        }

        pub fn getPtr() callconv(.C) *const anyopaque {
            return contents.ptr;
        }
    };

    @export(&Fns.getPtr, std.builtin.ExportOptions{ .name = "zig" ++ export_name ++ "Ptr" });
    @export(&Fns.getSize, std.builtin.ExportOptions{ .name = "zig" ++ export_name ++ "Size" });
}

comptime {
    exportFile("main.vert", "MainVert");
    exportFile("main.frag", "MainFrag");
    exportFile("texture.png", "Texture");
}
