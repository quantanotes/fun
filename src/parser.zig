const std = @import("std");
const any = @import("any.zig");
const Context = @import("ctx.zig").Context;
const Value = any.Value;

pub const Parser = struct {
    const Error = error{
        MismatchedParentheses,
    };

    const Quoted = enum(usize) {
        Quote,
        Quasi,
        Unquote,
        Splicing,
    };

    const QUOTED_SYMBOLS = [_]*const [*]u8{
        "quote",
        "quasi",
        "unquote",
        "splicing",
    };

    const SPECIAL = "()'`,@";
    const TOKEN_SIZE = 1024;

    allocator: std.mem.Allocator,

    buffer: [:0]const u8,
    bp: *const u8,
    token: [TOKEN_SIZE:0]u8,

    pub fn init(ctx: *Context) Parser {
        return Parser{
            .ctx = ctx,
            .buffer = undefined,
            .bp = undefined,
            .token = undefined,
        };
    }

    pub fn parse(self: *Parser, buffer: [:0]const u8) Value {
        self.buffer = buffer;
        self.bp = &buffer[0];
        self.next_token();

        return self.parse_exp();
    }

    pub fn parse_exp(self: *Parser) !Value {
        return switch (self.token[0]) {
            '(' => self.parse_list(),
            '\'' => self.parse_quoted(Quoted.Quote),
            '`' => self.parse_quoted(Quoted.Quasi),
            ',' => self.parse_quoted(Quoted.Unquote),
            '@' => self.parse_quoted(Quoted.Splicing),
            else => self.parse_atom(),
        };
    }

    fn parse_atom(self: *Parser) !Value {
        const integer = std.fmt.parseInt(i60, self.token, 10);
        if (integer) |i| {
            return Value{ .integer = i };
        }

        const float = std.fmt.parseFloat(f32, self.token);
        if (float) |f| {
            return Value{ .float = f };
        }

        return Value{};
    }

    fn parse_list(self: *Parser) !Value {
        self.next_token();
    }

    fn parse_quoted(self: *Parser, quoted: Quoted) !Value {
        _ = quoted; // autofix
        self.next_token();
    }

    fn next_token(self: *Parser) void {
        self.skip_ignored();

        self.token[0] = self.bp.*;
        self.bp += 1;

        var i: usize = 1;
        while (is_atom(self.bp.*)) : ({
            i += 1;
            self.bp += 1;
        }) {
            self.token[i] = self.bp.*;
        }
    }

    inline fn skip_ignored(self: *Parser) void {
        while (is_whitespace(self.bp.*) or self.bp.* == ';') {
            if (self.bp.* == ';') {
                self.skip_comment();
            } else {
                self.bp += 1;
            }
        }
    }

    inline fn skip_comment(self: *Parser) void {
        while (!is_eol(self.bp.*)) {
            self.bp += 1;
        }
    }

    inline fn token_len(self: *Parser) usize {
        var i: usize = 0;
        while (self.token[i] != 0) : (i += 1) {}

        return i;
    }

    inline fn is_whitespace(char: u8) bool {
        return char == ' ' or char == '\n' or char == '\t';
    }

    inline fn is_special(char: u8) bool {
        inline for (SPECIAL) |c| {
            if (char == c) return true;
        }

        return false;
    }

    inline fn is_eol(char: u8) bool {
        return char == 0 or char == '\n';
    }

    inline fn is_eof(char: u8) bool {
        return char == 0;
    }

    inline fn is_atom(char: u8) bool {
        return !is_eof(char) and !is_whitespace(char) and !is_special(char);
    }
};

// pub const AST = union(enum) {
//     integer: i32,
//     real: f32,
//     symbol: []u8,
//     list: []AST,
// };

// pub const ParserAst = struct {
//     const Error = error{
//         MismatchedParentheses,
//     };

//     const Quoted = enum(usize) {
//         Quote,
//         Quasi,
//         Unquote,
//         Splicing,
//     };

//     const QUOTED_SYMBOLS = [_]*const [*]u8{
//         "quote",
//         "quasi",
//         "unquote",
//         "splicing",
//     };

//     const SPECIAL = "()'`,@";
//     const TOKEN_SIZE = 1024;

//     allocator: std.mem.Allocator,

//     buffer: [:0]const u8,
//     bp: *const u8,
//     token: [TOKEN_SIZE:0]u8,

//     pub fn init(arena: *std.heap.ArenaAllocator) Parser {
//         var allocator = arena.allocator();

//         return Parser{
//             .allocator = allocator,
//             .buffer = undefined,
//             .bp = undefined,
//             .token = undefined,
//         };
//     }

//     pub fn parse(self: *Parser, buffer: [:0]const u8) !AST {
//         self.buffer = buffer;
//         self.bp = &buffer[0];
//         self.next_token();

//         return self.parse_exp();
//     }

//     pub fn parse_exp(self: *Parser) !AST {
//         return switch (self.token[0]) {
//             '(' => self.parse_list(),
//             '\'' => self.parse_quoted(Quoted.Quote),
//             '`' => self.parse_quoted(Quoted.Quasi),
//             ',' => self.parse_quoted(Quoted.Unquote),
//             '@' => self.parse_quoted(Quoted.Splicing),
//             else => self.parse_atom(),
//         };
//     }

//     fn parse_atom(self: *Parser) !AST {
//         const symbol = self.allocator.alloc(u8, self.token_len());

//         return .AST{ .symbol = symbol };
//     }

//     fn parse_list(self: *Parser) !AST {
//         self.next_token();

//         const list = std.ArrayList(AST).init(self.arena.allocator());

//         while (self.token[0] != ')' or self.token[0] == 0) {
//             list.append(self.parse_exp());
//         }

//         if (is_eof(self.token[0])) {
//             return Error.MismatchedParentheses;
//         }

//         return .AST{ .list = list.items };
//     }

//     fn parse_quoted(self: *Parser, quoted: Quoted) !AST {
//         self.next_token();

//         const symbol = QUOTED_SYMBOLS[quoted];
//         const exp = try self.parse_exp();
//         var list = try self.allocator.alloc(AST, 2);

//         list.* = []AST{ symbol, exp };

//         return .AST{ .list = list };
//     }

//     fn next_token(self: *Parser) void {
//         self.skip_ignored();

//         self.token[0] = self.bp.*;
//         self.bp += 1;

//         var i: usize = 1;
//         while (is_atom(self.bp.*)) : ({
//             i += 1;
//             self.bp += 1;
//         }) {
//             self.token[i] = self.bp.*;
//         }
//     }

//     inline fn skip_ignored(self: *Parser) void {
//         while (is_whitespace(self.bp.*) or self.bp.* == ';') {
//             if (self.bp.* == ';') {
//                 self.skip_comment();
//             } else {
//                 self.bp += 1;
//             }
//         }
//     }

//     inline fn skip_comment(self: *Parser) void {
//         while (!is_eol(self.bp.*)) {
//             self.bp += 1;
//         }
//     }

//     inline fn token_len(self: *Parser) usize {
//         var i: usize = 0;
//         while (self.token[i] != 0) : (i += 1) {}
//         return i;
//     }

//     inline fn is_whitespace(char: u8) bool {
//         return char == ' ' or char == '\n' or char == '\t';
//     }

//     inline fn is_special(char: u8) bool {
//         inline for (SPECIAL) |c| {
//             if (char == c) return true;
//         }

//         return false;
//     }

//     inline fn is_eol(char: u8) bool {
//         return char == 0 or char == '\n';
//     }

//     inline fn is_eof(char: u8) bool {
//         return char == 0;
//     }

//     inline fn is_atom(char: u8) bool {
//         return !is_eof(char) and !is_whitespace(char) and !is_special(char);
//     }
// };
