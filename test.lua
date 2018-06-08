local lak = require "odielak";

local sanityt = {
	['&'] = '&amp;',
	['<'] = '&lt;',
	['>'] = '&gt;',
	['"'] = '&qout;',
	['\''] = '&#x27;',
	['/'] = '&#x2F;',
	['A'] = '', -- option (replace A with nothing)
	['H'] = {}, -- ignore
	[255] = 'last',
	['U'] = setmetatable({}, {__tostring = function() end}), -- ignore
	['B'] = setmetatable({"CC"}, {__tostring = function(self) return self[1]; end}),
	['F'] = function(self, str, key) print(self, str, key); return '_F_'; end,
	[256] = 'bad key',
}

local str = 'abc234;&&&12<*A*>  ///12O"A\'-9 \n\tOL&';

-- lak
local sanity_it = lak:New(sanityt);

local lstr = sanity_it(str);
local mstr = sanity_it(setmetatable({ str = str }, {__tostring = function(self)
	return self.str;
end}));

-- gsub
local sanityr = '[&<>"\'/A]';

local gstr = string.gsub(str, sanityr, sanityt);

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
assert(lak:New({})("012345") == "012345");
assert(lak:New({[1] = '1'})("012345") == "012345");
