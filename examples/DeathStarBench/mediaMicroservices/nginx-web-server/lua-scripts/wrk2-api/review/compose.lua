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

local function _UploadUserId(req_id, post, carrier)
  local GenericObjectPool = require "GenericObjectPool"
  local UserServiceClient = require 'media_service_UserService'
  local max_retries = 10
  local attempt = 0
  local success = false
  local client, err

  while attempt < max_retries do
    attempt = attempt + 1
    client, err = GenericObjectPool:connection(UserServiceClient, "user-service" .. k8s_suffix, 9090)

    if client then
      local ok, upload_err = pcall(function()
        client:UploadUserWithUsername(req_id, post.username, carrier)
      end)

      if ok then
        success = true
        GenericObjectPool:returnConnection(client)
        break
      else
        ngx.log(ngx.ERR, "Attempt " .. attempt .. " failed: " .. (upload_err or "unknown error"))
      end
    else
      ngx.log(ngx.ERR, "Failed to obtain connection on attempt " .. attempt .. ": " .. (err or "unknown error"))
    end

    if attempt < max_retries then
      sleep(1)
    end
  end

  if not success then
    ngx.log(ngx.ERR, "UserService is unavailable after retries.")
    return false
  end
  return true
end

local function _UploadText(req_id, post, carrier)
  local GenericObjectPool = require "GenericObjectPool"
  local TextServiceClient = require 'media_service_TextService'
  local max_retries = 10
  local attempt = 0
  local success = false
  local client, err

  while attempt < max_retries do
    attempt = attempt + 1
    client, err = GenericObjectPool:connection(TextServiceClient, "text-service" .. k8s_suffix, 9090)

    if client then
      local ok, upload_err = pcall(function()
        client:UploadText(req_id, post.text, carrier)
      end)

      if ok then
        success = true
        GenericObjectPool:returnConnection(client)
        break
      else
        ngx.log(ngx.ERR, "Attempt " .. attempt .. " failed: " .. (upload_err or "unknown error"))
      end
    else
      ngx.log(ngx.ERR, "Failed to obtain connection on attempt " .. attempt .. ": " .. (err or "unknown error"))
    end

    if attempt < max_retries then
      sleep(1)
    end
  end

  if not success then
    ngx.log(ngx.ERR, "TextService is unavailable after retries.")
    return false
  end
  return true
end

local function _UploadMovieId(req_id, post, carrier)
  local GenericObjectPool = require "GenericObjectPool"
  local MovieIdServiceClient = require 'media_service_MovieIdService'
  local max_retries = 10
  local attempt = 0
  local success = false
  local client, err

  while attempt < max_retries do
    attempt = attempt + 1
    client, err = GenericObjectPool:connection(MovieIdServiceClient, "movie-id-service" .. k8s_suffix, 9090)

    if client then
      local ok, upload_err = pcall(function()
        client:UploadMovieId(req_id, post.title, tonumber(post.rating), carrier)
      end)

      if ok then
        success = true
        GenericObjectPool:returnConnection(client)
        break
      else
        ngx.log(ngx.ERR, "Attempt " .. attempt .. " failed: " .. (upload_err or "unknown error"))
      end
    else
      ngx.log(ngx.ERR, "Failed to obtain connection on attempt " .. attempt .. ": " .. (err or "unknown error"))
    end

    if attempt < max_retries then
      sleep(1)
    end
  end

  if not success then
    ngx.log(ngx.ERR, "MovieIdService is unavailable after retries.")
    return false
  end
  return true
end

local function _UploadUniqueId(req_id, carrier)
  local GenericObjectPool = require "GenericObjectPool"
  local UniqueIdServiceClient = require 'media_service_UniqueIdService'
  local max_retries = 10
  local attempt = 0
  local success = false
  local client, err

  while attempt < max_retries do
    attempt = attempt + 1
    client, err = GenericObjectPool:connection(UniqueIdServiceClient, "unique-id-service" .. k8s_suffix, 9090)

    if client then
      local ok, upload_err = pcall(function()
        client:UploadUniqueId(req_id, carrier)
      end)

      if ok then
        success = true
        GenericObjectPool:returnConnection(client)
        break
      else
        ngx.log(ngx.ERR, "Attempt " .. attempt .. " failed: " .. (upload_err or "unknown error"))
      end
    else
      ngx.log(ngx.ERR, "Failed to obtain connection on attempt " .. attempt .. ": " .. (err or "unknown error"))
    end

    if attempt < max_retries then
      sleep(1)
    end
  end

  if not success then
    ngx.log(ngx.ERR, "UniqueIdService is unavailable after retries.")
    return false
  end
  return true
end

function _M.ComposeReview()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local ngx = ngx

  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(ngx.var.opentracing_binary_context)
  local span = tracer:start_span("ComposeReview", {["references"] = {{"child_of", parent_span_context}}})
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local post = ngx.req.get_post_args()

  if (_StrIsEmpty(post.title) or _StrIsEmpty(post.text) or
      _StrIsEmpty(post.username) or _StrIsEmpty(post.password) or
      _StrIsEmpty(post.rating)) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end

  local results = {
    _UploadUserId(req_id, post, carrier),
    _UploadMovieId(req_id, post, carrier),
    _UploadText(req_id, post, carrier),
    _UploadUniqueId(req_id, carrier)
  }

  local success = true
  for _, result in ipairs(results) do
    if not result then
      success = false
    end
  end

  if not success then
    ngx.status = ngx.HTTP_SERVICE_UNAVAILABLE
    ngx.say("One or more services are unavailable. Please try again later.")
    ngx.log(ngx.ERR, "ComposeReview failed due to service unavailability.")
  end

  span:finish()
  ngx.exit(success and ngx.HTTP_OK or ngx.HTTP_SERVICE_UNAVAILABLE)
end

return _M
