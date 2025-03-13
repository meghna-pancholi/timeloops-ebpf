#!/bin/bash

docker build -f Dockerfile -t mm-loadgenerator . #--no-cache

IMAGE_ID=$(docker images --format="{{.Repository}} {{.ID}}" | grep "^mm-loadgenerator " | cut -d' ' -f2)

docker tag $IMAGE_ID 520842413394.dkr.ecr.us-east-1.amazonaws.com/loadgenerator:media
docker push 520842413394.dkr.ecr.us-east-1.amazonaws.com/loadgenerator:media
