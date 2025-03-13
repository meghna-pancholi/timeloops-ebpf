# FROM yg397/thrift-microservice-deps:xenial
FROM thrift-microservice-deps:latest

COPY ./ /media-microservices
RUN cd /media-microservices \
    && mkdir -p build \
    && cd build \
    && cmake .. \
    && make \
    && make install

WORKDIR /media-microservices