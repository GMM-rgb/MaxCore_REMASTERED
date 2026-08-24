local options <const> = {"YOU WON ${BV}!", "you lost, womp womp!"}

-- AUTHOR: Jones H.

while true do
    math.randomseed(math.ceil(os.time() / math.random(os.time())))
    io.stdout:write("BET VALUE:\t")
    local value <const> = io.read()
    local randomindex <const> = math.random(#options)
    local formated = options[randomindex]:gsub("{BV}", value * 1.5 + math.random(0, 100.0))
    print(tostring(formated))
end
