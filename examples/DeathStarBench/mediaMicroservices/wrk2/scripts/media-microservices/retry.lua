local retry_limit = 5  -- Maximum retry attempts per request
local retry_counts = {}  -- Track retries per thread

-- Character set for random string generation
charset = {}  
for c = 48, 57  do table.insert(charset, string.char(c)) end  -- 0-9
for c = 65, 90  do table.insert(charset, string.char(c)) end  -- A-Z
for c = 97, 122 do table.insert(charset, string.char(c)) end  -- a-z

function string.random(length)
  if length > 0 then
    return string.random(length - 1) .. charset[math.random(1, #charset)]
  else
    return ""
  end
end

-- Generate a request
request = function()
  local movie_index = math.random(1000)
  local user_index = math.random(1000)
  local username = "username_" .. tostring(user_index)
  local password = "password_" .. tostring(user_index)
  local title = "title_" .. tostring(movie_index)
  local rating = math.random(0, 10)
  local text = string.random(256)

  local path = url .. "/wrk2-api/review/compose"
  local method = "POST"
  local headers = {["Content-Type"] = "application/x-www-form-urlencoded"}
  local body = "username=" .. username .. "&password=" .. password .. "&title=" ..
                  title .. "&rating=" .. rating .. "&text=" .. text

  return wrk.format(method, path, headers, body)
end

-- URL encoding function
function urlEncode(s)
  s = string.gsub(s, "([^%w%.%- ])", function(c) return string.format("%%%02X", string.byte(c)) end)
  return string.gsub(s, " ", "+")
end

-- Handle responses and retry if needed
function response(status, headers, body)
  local key = wrk.thread.addr  -- Unique key per thread
  retry_counts[key] = retry_counts[key] or 0

  -- Retry logic: If response is not HTTP 200, retry until limit is reached
  if status ~= 200 and retry_counts[key] < retry_limit then
    retry_counts[key] = retry_counts[key] + 1
    wrk.request = request()  -- Requeue request
  else
    retry_counts[key] = 0  -- Reset retry count on success
  end
end
