pub const std = @import("std");
pub const GC = @import("gc.zig").GC;

const NIL = 0;
const VOID = 1;

pub const Value = union(enum) {
    reference: Reference,
    boolean: bool,
    integer: i60,
    real: f32,
    special: u2,

    pub fn toString(self: Value, buffer: []u8) ![]const u8 {
        return switch (self) {
            .boolean => if (self.boolean) "true" else "false",
            .integer => std.fmt.bufPrint(buffer, "{}", self.integer),
            .real => std.fmt.bufPrint(buffer, "{}", self.real),
            .special => if (self.special == 0) "nil" else "void",
            else => buffer,
        };
    }
};

pub const Object = union(enum) {
    reference: Reference,
    boolean: bool,
    integer: u64,
    real: f64,
    pair: *Pair,

    pub fn typeToTag(comptime T: type) ?u4 {
        inline for (std.meta.fields(Object), 0..) |field, i| {
            if (T == field.type) {
                return i;
            }
        }

        return null;
    }

    pub fn tagToType(comptime tag: u4) ?type {
        inline for (std.meta.fields(Object), 0..) |field, i| {
            if (tag == i) {
                return field.type;
            }
        }

        return null;
    }
};

test "object type to tag" {
    const tag = Object.typeToTag(*Pair);
    try std.testing.expectEqual(tag.?, 4);
}

test "object tag to type" {
    const Type = Object.tagToType(4);
    try std.testing.expectEqual(Type.?, *Pair);
}

pub const Block = packed struct {
    tag: u4,
    size: u32,
    many: bool,

    pub fn init(gc: *GC, tag: u4, size: u32, many: bool) !*Block {
        const pntr = try gc.alloc(size + @sizeOf(Block));
        const block = @as(*Block, @alignCast(@ptrCast(pntr[0..@sizeOf(Block)])));

        block.*.tag = tag;
        block.*.size = size;
        block.*.many = many;

        return block;
    }

    pub inline fn ptr(self: *Block) [*]u8 {
        return @as([*]u8, @ptrCast(self)) + @sizeOf(Block);
    }

    pub inline fn raw(self: *Block) []u8 {
        return self.ptr()[0..self.len()];
    }

    // Assume first 32 bits of many object is a length
    pub inline fn len(self: *Block) u32 {
        if (!self.many) {
            return self.size;
        } else {
            const many_len = std.mem.readIntNative(u32, self.ptr()[0..@sizeOf(u32)]);

            return self.size * many_len;
        }
    }

    pub fn obj(self: *Block) ?Object {
        inline for (std.meta.fields(Object), 0..) |field, i| {
            if (self.tag == i) {
                return @unionInit(Object, field.name, @as(*field.type, @alignCast(@ptrCast(self.raw()))).*);
            }
        }

        return null;
    }

    pub fn ref(self: *Block) Reference {
        _ = self; // autofix

    }
};

pub const Meta = packed struct {
    tag: u4,
    size: u32,
    many: bool,

    pub fn reflect(comptime T: type) ?Meta {
        var tag: ?u4 = null;
        var size: u32 = 0;
        var many = false;

        switch (@typeInfo(T)) {
            .Array => |info| {
                const is_value = @sizeOf(info.child) > @sizeOf(u64);

                tag = Object.typeToTag(if (is_value) *info.child else info.child);
                size = @sizeOf(T) * info.len;
                many = true;
            },
            else => {
                const is_value = @sizeOf(T) > @sizeOf(u64);

                tag = Object.typeToTag(if (is_value) *T else T);
                size = @sizeOf(T);
            },
        }

        return if (tag != null) Meta{ .tag = tag.?, .size = size, .many = many } else null;
    }
};

test "init block" {
    const allocator = std.testing.allocator;

    var gc = try GC.init(allocator);
    defer gc.deinit();

    const block = try Block.init(&gc, 10, 1, true);
    _ = block;
}

test "block object" {
    const allocator = std.testing.allocator;

    var gc = try GC.init(allocator);
    defer gc.deinit();

    const block = try Block.init(&gc, 0, 1, true);

    _ = block.obj();
}

pub const Reference = packed struct {
    weak: bool,
    ptr: u58,
};

pub const Pair = struct {
    car: Value,
    cdr: Value,
};

test "create pair" {
    const allocator = std.testing.allocator;
    var gc = try GC.init(allocator);
    defer gc.deinit();

    const pair = try gc.create([10]Pair);
    _ = pair; // autofix
}
