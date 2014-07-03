util = {}

-- lighting = false
lighting = true

function util.hostname()
  if os.execute("hostname > .hostname-tmp") == 0 then
    return "'hostname' execution failed"
  end
  f = io.open(".hostname-tmp", "r")
  if f == nil then return "could-not-open-file" end
  return f:read("*l")
end

function util.LoadDS(filename, targetBrickSize, resolution)
  if not util.FileExists(filename) then
    print("no such file: " .. filename)
    return nil
  end

  -- rw = iv3d.renderer.new(datasets.Dummy) -- we do not want the dummy
  -- here because we want to load the right TF, hopefully that won't
  -- affect the systemIO cache thing

  rw = iv3d.renderer.new(filename)
  -- rw.tfqn1d(string.gsub(filename, 'uvf', '1dt')) -- causes sometimes
  -- LuaStackRAII: unexpected stack size
  rw.lighting(lighting)
  rw.resize(resolution)
  rawr = rw.getRawRenderer()
  rawr.loadRebricked(filename, {targetBrickSize, targetBrickSize,
                                targetBrickSize}, MM_PRECOMPUTE)
  --rawr.setDebugView(2)

  return rw
end

function util.LoadDSNoRebrick(filename, targetBrickSize, resolution)
  if not util.FileExists(filename) then return nil end
  rw = iv3d.renderer.new(filename)
  --rw.tfqn1d(string.gsub(filename, 'uvf', '1dt')) -- causes sometimes LuaStackRAII: unexpected stack size 
  rw.lighting(lighting)
  rw.resize(resolution)
  rawr = rw.getRawRenderer()
  --rawr.setDebugView(2)

  return rw
end

function util.LoadDSNoCache(filename, targetBrickSize, resolution)
  if not util.FileExists(filename) then return nil end
  --rw = iv3d.renderer.new(datasets.Dummy) -- we do not want the dummy here because we want to load the right TF, hopefully that won't affect the systemIO cache thing
  rw = iv3d.renderer.new(filename)
  --rw.tfqn1d(string.gsub(filename, 'uvf', '1dt')) -- causes sometimes LuaStackRAII: unexpected stack size
  rw.lighting(lighting)
  rw.resize(resolution)
  rawr = rw.getRawRenderer()
  rawr.loadRebricked(filename, {targetBrickSize, targetBrickSize, targetBrickSize}, MM_PRECOMPUTE)
  --rawr.setDebugView(2)
  rawr.getDataset().setCacheSize(0)
  return rw
end

function util.ClearPerfCounters()
  -- just requesting the value implicitly clears it.
  tuvok.perf(PERF_SUBFRAMES)
  tuvok.perf(PERF_RENDER)
  tuvok.perf(PERF_RAYCAST)
  tuvok.perf(PERF_READ_HTABLE)
  tuvok.perf(PERF_CONDENSE_HTABLE)
  tuvok.perf(PERF_SORT_HTABLE)
  tuvok.perf(PERF_UPLOAD_BRICKS)
  tuvok.perf(PERF_POOL_SORT)
  tuvok.perf(PERF_POOL_UPLOADED_MEM)
  tuvok.perf(PERF_POOL_GET_BRICK)
  tuvok.perf(PERF_DY_GET_BRICK)
  tuvok.perf(PERF_DY_CACHE_LOOKUPS)
  tuvok.perf(PERF_DY_CACHE_LOOKUP)
  tuvok.perf(PERF_DY_LOAD_BRICK)
  tuvok.perf(PERF_DY_CACHE_ADDS)
  tuvok.perf(PERF_DY_CACHE_ADD)
  tuvok.perf(PERF_DY_BRICK_COPY)
  tuvok.perf(PERF_POOL_UPLOAD_BRICK)
  tuvok.perf(PERF_POOL_UPLOAD_TEXEL)
  tuvok.perf(PERF_POOL_UPLOAD_METADATA)
  tuvok.perf(PERF_EO_BRICKS)
  tuvok.perf(PERF_EO_DISK_READ)
  tuvok.perf(PERF_EO_DECOMPRESSION)
end

function util.GetDynBrickCacheMem(rw)
  if (rw.getRawRenderer().getDataset().getCacheSize ~= nil) then
    return rw.getRawRenderer().getDataset().getCacheSize()
  end
  return 0
end

function util.FlushSystemIOCache()
  -- implement me!
end

function util.CleanMem()
  kilo, junk = collectgarbage("count")
  repeat 
    prev = kilo
    collectgarbage("collect")
    kilo, junk = collectgarbage("count")
  until prev == kilo
end

function util.FileExists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end
end

file = nil

function util.SetOutputFile(filename)
  file = io.open(filename, "a+")
  print("# set output file: " .. filename)
end

function util.printf(s,...)
  if (file ~= nil) then
    file:write(s:format(...))
    file:flush()
  end
  io.write(s:format(...))
  io.flush()
end

function util.printHeader()
  util.printf("# %s;%s;%s;%s;%s;%s;%s;",
              "filename",
              "brickSize",
              "scenario",
              "device",
              "description",
              "resolutionX",
              "resolutionY")
  util.printf("%s;%s;%s;%s;%s;%s;%s;%s;",
              "hashTableSize",
              "rehashCount",
              "brickStrategy",
              "mdUpdateStrategy",
              "gpuMem (MB)",
              "cpuMem (MB)",
              "dynBrickCacheMem (MB)",
              "frameCount")
  util.printf("%s;%s;%s;%s;%s;%s;%s;",
              "SUBFRAMES",
              "RENDER",
                "RAYCAST",
                "READ_HTABLE",
                "CONDENSE_HTABLE",
                "SORT_HTABLE",
                "UPLOAD_BRICKS")
  util.printf("%s;%s;%s;%s;",
                  "POOL_SORT",
                  "POOL_UPLOADED_MEM (Bytes)",
                  "POOL_GET_BRICK",
                    "DY_GET_BRICK")
  util.printf("%s;%s;%s;%s;%s;%s;%s;%s;",
                      "DY_CACHE_LOOKUPS",
                      "DY_CACHE_LOOKUP",
                      "DY_RESERVE_BRICK",
                      "DY_LOAD_BRICK",
                      "DY_CACHE_ADDS",
                      "DY_CACHE_ADD",
                      "DY_BRICK_COPIED",
                      "DY_BRICK_COPY")
  util.printf("%s;%s;%s;%s;%s;%s\n",
                  "POOL_UPLOAD_BRICK",
                    "POOL_UPLOAD_TEXEL",
                  "POOL_UPLOAD_METADATA",
                  "EO_BRICKS",
                  "EO_DISK_READ",
                  "EO_DECOMPRESSION")
end

function util.printMeasures(filename, targetBrickSize, resolution, scenario, device, description, frameCount)
  util.printf("%s;%.0f;%s;%s;%s;%.0f;%.0f;",
              filename,
              targetBrickSize,
              scenario,
              device,
              description,
              resolution[1],
              resolution[2])
  util.printf("%.0f;%.0f;%.0f;%.0f;%.0f;%.0f;%.0f;%.0f;",
              tuvok.state.getHashTableSize(),
              tuvok.state.getRehashCount(),
              tuvok.state.getBrickStrategy(),
              tuvok.state.getMdUpdateStrategy(),
              tuvok.state.getGpuMem(),
              tuvok.state.getCpuMem(),              
              util.GetDynBrickCacheMem(rw),
              frameCount)
  util.printf("%.0f;%.4f;%.4f;%.4f;%.4f;%.4f;%.4f;",
              tuvok.perf(PERF_SUBFRAMES),
              tuvok.perf(PERF_RENDER),
                tuvok.perf(PERF_RAYCAST),
                tuvok.perf(PERF_READ_HTABLE),
                tuvok.perf(PERF_CONDENSE_HTABLE),
                tuvok.perf(PERF_SORT_HTABLE),
                tuvok.perf(PERF_UPLOAD_BRICKS))
  util.printf("%.4f;%.0f;%.4f;%.4f;",
                  tuvok.perf(PERF_POOL_SORT),
                  tuvok.perf(PERF_POOL_UPLOADED_MEM),
                  tuvok.perf(PERF_POOL_GET_BRICK),
                    tuvok.perf(PERF_DY_GET_BRICK))
  util.printf("%.0f;%.4f;%.4f;%.4f;%.0f;%.4f;%.0f;%.4f;",
                      tuvok.perf(PERF_DY_CACHE_LOOKUPS),
                      tuvok.perf(PERF_DY_CACHE_LOOKUP),
                      tuvok.perf(PERF_DY_RESERVE_BRICK),
                      tuvok.perf(PERF_DY_LOAD_BRICK),
                      tuvok.perf(PERF_DY_CACHE_ADDS),
                      tuvok.perf(PERF_DY_CACHE_ADD),
                      tuvok.perf(PERF_DY_BRICK_COPIED),
                      tuvok.perf(PERF_DY_BRICK_COPY))
  util.printf("%.4f;%.4f;%.4f;%.0f;%.4f;%.4f\n",
                  tuvok.perf(PERF_POOL_UPLOAD_BRICK),
                    tuvok.perf(PERF_POOL_UPLOAD_TEXEL),
                  tuvok.perf(PERF_POOL_UPLOAD_METADATA),
                  tuvok.perf(PERF_EO_BRICKS),
                  tuvok.perf(PERF_EO_DISK_READ),
                  tuvok.perf(PERF_EO_DECOMPRESSION))
end

return util
