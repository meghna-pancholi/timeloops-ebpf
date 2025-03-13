local _M = {}
local k8s_suffix = os.getenv("fqdn_suffix")
if (k8s_suffix == nil) then
  k8s_suffix = ""
end

local function _StrIsEmpty(s)
  return s == nil or s == ''
end

local function sleep(n)
  ngx.sleep(n)  -- Delay execution for n seconds
end

function _M.RegisterUser()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local GenericObjectPool = require "GenericObjectPool"
  local UserServiceClient = require 'media_service_UserService'
  local ngx = ngx

  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(ngx.var.opentracing_binary_context)
  local span = tracer:start_span("RegisterUser", {["references"] = {{"child_of", parent_span_context}}})
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local post = ngx.req.get_post_args()

  if (_StrIsEmpty(post.first_name) or _StrIsEmpty(post.last_name) or
      _StrIsEmpty(post.username) or _StrIsEmpty(post.password)) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  local max_retries = 10 
  local attempt = 0
  local success = false
  local client, err

  while attempt < max_retries do
    attempt = attempt + 1
    client, err = GenericObjectPool:connection(UserServiceClient, "user-service" .. k8s_suffix, 9090)

    if client then
      local ok, register_err = pcall(function()
        client:RegisterUser(req_id, post.first_name, post.last_name, post.username, post.password, carrier)
      end)

      if ok then
        success = true
        GenericObjectPool:returnConnection(client)
        break
      else
        ngx.log(ngx.ERR, "Attempt " .. attempt .. " failed: " .. (register_err or "unknown error"))
      end
    else
      ngx.log(ngx.ERR, "Failed to obtain connection on attempt " .. attempt .. ": " .. (err or "unknown error"))
    end

    if attempt < max_retries then
      sleep(1)  -- Wait 1 second before retrying
    end
  end

  if not success then
    ngx.status = ngx.HTTP_SERVICE_UNAVAILABLE
    ngx.say("UserService is unavailable. Please try again later.")
    ngx.exit(ngx.HTTP_SERVICE_UNAVAILABLE)
  end

  span:finish()
end

return _M

