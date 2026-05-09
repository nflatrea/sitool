local H = {}
H.commands  = {}
H.callbacks = {}

H.callbacks.on_recv = function(raw)
    if #raw < 1 then return end
    -- try to identify by first byte patterns
    local hex = sitool.utils.btoh(raw)
	if string.sub(hex, 1, string.len("AA BB CC")) == "AA BB CC" then
	    print("String starts with prefix AA BB CC\r\n")
	end
    printf("[dissect] len=%d data=%s\r\n", #raw, hex)
end

H.callbacks.on_load = function()
    print("[test_sniff] Handler loaded")
    print("[test_sniff] Type \"help\" for help")
end

H.callbacks.on_unload = function()
    print("[test_sniff] Handler unloaded")
end


return H
