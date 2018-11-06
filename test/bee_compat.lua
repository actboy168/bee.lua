for _, v in ipairs {
    'subprocess',
    'filesystem',
    'registry',
} do
    package.loaded[v] = require('bee.' .. v)
end
fs = package.loaded.filesystem
