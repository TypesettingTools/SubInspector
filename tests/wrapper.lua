local wrapper, include = arg[1], arg[2]
local ffi = require('ffi')

local function load_wrapper(test_ffi, search_path, depctrl)
    local registered
    local env = setmetatable({
        package = {path = search_path},
        aegisub = {
            decode_path = function() return include .. '/fonts' end,
            video_size = function() return nil end,
            log = function() end,
        },
        require = function(name)
            if name == 'ffi' then return test_ffi end
            if name == 'l0.DependencyControl' and depctrl then
                return function(record)
                    record.requireModules = function(self)
                        assert(#self[1] == 1 and self[1][1][1] == 'ffi')
                        return test_ffi
                    end
                    record.checkVersion = function() return true end
                    record.register = function(_, module)
                        registered = module
                        return module
                    end
                    return record
                end
            end
            error('Unexpected dependency: ' .. name)
        end,
    }, {__index = _G})
    local module = setfenv(assert(loadfile(wrapper)), env)()
    if depctrl then assert(registered == module) end
    return module
end

-- Exercise all platform names and path conventions without loading foreign
-- binaries. These cases also run without DependencyControl or requireffi.
local cases = {
    {'Windows', 'C:\\Users\\Test Person\\include\\?.lua',
     'C:/Users/Test Person/include/SubInspector/Inspector/SubInspector.dll'},
    {'OSX', '/missing/?.lua;/Application Support/include/?.moon',
     '/Application Support/include/SubInspector/Inspector/libSubInspector.dylib'},
    {'Linux', '/usr/share/aegisub/automation/include/?.lua',
     '/usr/share/aegisub/automation/include/SubInspector/Inspector/libSubInspector.so'},
    {'Linux', '/missing/?.lua;/missing/?/init.lua', 'SubInspector'},
}
for _, case in ipairs(cases) do
    for _, depctrl in ipairs({false, true}) do
        local calls = {}
        load_wrapper({
            os = case[1],
            cdef = function() end,
            load = function(path)
                calls[#calls + 1] = path
                if path == case[3] then return {} end
                error('library missing or incompatible')
            end,
        }, case[2], depctrl)
        assert(calls[#calls] == case[3])
        for _, path in ipairs(calls) do
            assert(not path:find('init.lua', 1, true))
        end
    end
end

local ok, message = pcall(load_wrapper, {
    os = 'OSX',
    cdef = function() end,
    load = function() error('wrong architecture') end,
}, '/missing/?.lua', false)
assert(not ok)
assert(message:find('/missing/SubInspector/Inspector/libSubInspector.dylib', 1, true))
assert(message:find('SubInspector: ', 1, true))
assert(message:find('wrong architecture', 1, true))

-- Load the real staged library and exercise the wrapper-to-C interface, both
-- with and without DependencyControl registration.
local declared = false
for _, depctrl in ipairs({false, true}) do
    local loaded_path, config
    local test_ffi = setmetatable({
        cdef = function(definitions)
            if not declared then ffi.cdef(definitions); declared = true end
        end,
        load = function(path)
            local library = ffi.load(path)
            loaded_path = path
            return setmetatable({
                si_init = function(width, height, font_config, fonts)
                    config = font_config
                    return library.si_init(width, height, font_config, fonts)
                end,
            }, {__index = function(_, key) return library[key] end})
        end,
    }, {__index = ffi})
    local Inspector = load_wrapper(test_ffi, include .. '/?.lua', depctrl)
    local expected_root = include:gsub('\\', '/') .. '/SubInspector/Inspector/'
    assert(loaded_path:find(expected_root, 1, true) == 1)
    local subtitles = {
        {class = 'info', key = 'ScriptType', value = 'v4.00+', raw = 'ScriptType: v4.00+'},
        {class = 'info', key = 'PlayResX', value = '640', raw = 'PlayResX: 640'},
        {class = 'info', key = 'PlayResY', value = '480', raw = 'PlayResY: 480'},
        {class = 'style', name = 'Default', raw =
            'Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,' ..
            '0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1'},
    }
    local inspector = Inspector(subtitles)
    assert(config == nil and inspector.fcConfig == nil)
    local shape = '{\\an7\\pos(100,100)\\p1}m 0 0 l 20 0 20 20 0 20'
    local rects = assert(inspector:getBounds({{
        style = 'Default', text = shape,
        raw = 'Dialogue: 0,0:00:00.00,0:00:02.00,Default,,0,0,0,,' .. shape,
    }}, {0, 500}))
    assert(#rects == 2)
    assert(rects[1].x == 100 and rects[1].y == 100)
    assert(rects[1].w == 20 and rects[1].h == 20 and rects[1].solid)
    assert(rects[1].hash == rects[2].hash)
    local configured = Inspector(subtitles, 'custom-fonts.conf')
    assert(config == 'custom-fonts.conf' and configured.fcConfig == config)
end

print('Library discovery, errors, DependencyControl registration, and wrapper rendering passed.')
