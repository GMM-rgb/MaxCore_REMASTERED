---@param s string
---@return table<string>
function string.split(s)
    local splitted = {}; for i = 1, #s do
        local ss = s:sub(i, i) or string.char(0)
        table.insert(splitted, ss)
    end; return splitted
end
