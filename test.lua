local lak = require "odielak";

local sanityt = {
	['&'] = '&amp;',
	['<'] = '&lt;',
	['>'] = '&gt;',
	['"'] = '&qout;',
	['H'] = {}, -- ignore
	[255] = 'last',
	['U'] = setmetatable({}, {__tostring = function() end}), -- ignore
	['B'] = setmetatable({"CC"}, {__tostring = function(self) return self[1]; end}),
	['F'] = function(self, str, key) return '_F_'; end,
	[256] = 'bad key',
}

local sanity_it2 = {
	['\''] = '&#x27;',
	['/'] = '&#x2F;',
	['A'] = '', -- option (replace A with nothing)
}

local g_table = {
	-- sanityt
	['&'] = '&amp;',
	['<'] = '&lt;',
	['>'] = '&gt;',
	['"'] = '&qout;',
	['H'] = {}, -- ignore
	[255] = 'last',
	['U'] = setmetatable({}, {__tostring = function() end}), -- ignore
	['B'] = setmetatable({"CC"}, {__tostring = function(self) return self[1]; end}),
	['F'] = function(self, str, key) print(self, str, key); return '_F_'; end,
	[256] = 'bad key',

	-- sanity_it2
	['\''] = '&#x27;',
	['/'] = '&#x2F;',
	['A'] = '', -- option (replace A with nothing)
}

local str = 'abc234;&&&12<*A*>  ///12O"A\'-9 \n\tOL&';

-- lak
local sanity_it = lak.new(sanityt, sanity_it2);

local lstr = sanity_it(str);
local mstr = sanity_it(setmetatable({ str = str }, {__tostring = function(self)
	return self.str;
end}));

-- gsub
local sanityr = '[&<>"\'/A]';

local gstr = string.gsub(str, sanityr, g_table);

if (lstr ~= gstr or mstr ~= gstr) then
	print("Lstr: >>" .. lstr .. "<<");
	print("Mstr: >>" .. mstr .. "<<");
	print("Gstr: >>" .. gstr .. "<<");
	error("NOT MATCHED!");
end

assert(sanity_it(("1234" .. string.char(255):rep(3))) == "1234lastlastlast");
assert(sanity_it("---BFABB---", 1, nil, false) == "---CC_F_CCCC---");
assert(sanity_it("  ") == "  ");
assert(sanity_it("  ", 10) == "  ");
assert(sanity_it(1, 2) == "1");
assert(sanity_it(nil) == nil);
assert(sanity_it(true) == nil);
assert(lak.new({})("012345") == "012345");
assert(lak.new({[1] = '1'})("012345") == "012345");

local big = ("8"):rep(3086) .. "_+";
local to = ("8"):rep(3086 + 3);
local l = lak.new({["_"] = "8", ["+"] = "88"});

assert(l(big) == to);

local x,y,z,y2,x2 = l(big, big..big, big..big..big, big..big, big);

assert(x == x2 and x2 == to, x2);
assert(y == y2 and y2 == to..to);
assert(z == to..to..to);

local f = function(self, str, k)
	assert(string.byte(str) == k);
	return '00';
end

assert(lak.new({
	['x'] = f;
	['y'] = f;
	['z'] = f;
})('x', 'y', 'z') == "00");
