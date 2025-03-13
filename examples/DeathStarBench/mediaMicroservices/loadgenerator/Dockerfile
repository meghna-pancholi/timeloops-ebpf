FROM python:3.8-slim as base

FROM base as builder

RUN apt-get -qq update \
    && apt-get install -y --no-install-recommends \
        g++ 

COPY requirements.txt .

RUN pip install --prefix="/install" -r requirements.txt

FROM base

RUN apt-get -qq update \
    && apt-get install -y --no-install-recommends \
        libssl-dev build-essential libz-dev 
        
        # luarocks

# RUN luarocks install luasocket

WORKDIR /loadgen

COPY --from=builder /install /usr/local

# Add application code.
COPY init_movies_users.py .
COPY datasets .
COPY locustfile.py .

# enable gevent support in debugger
ENV GEVENT_SUPPORT=True

ENTRYPOINT python3 init_movies_users.py --server_address http://nginx-web-server:8080 ; \
           sleep 60 ; \
        #    echo "Warmup start..." ; \
        #    locust --host="http://${FRONTEND_ADDR}" --headless -t 2m -u 10 2> /dev/null || true ; \
        #    echo "Warmup end..." ; \
           locust --host="http://${FRONTEND_ADDR}" --headless -t 10m -u "${USERS:-10}" > locust.out 2>&1 & \
           sleep infinity
