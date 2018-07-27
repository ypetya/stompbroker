# Docker

My steps to make it possible to run on other hosts

## Docker file

### Dockerfile.build 

The Dockerfile to make it possible to build the code
copy it to the projects root as `Dockerfile`

```
cp Dockerfile.build ../Dockerfile
```

## To build a docker conainter:

```
docker build -t kisp/stompbroker .
```

## To run a container with an exposed port:

```
docker run -p 3490:3490 kisp/stompbroker ./stompbroker.out
```

## To export a docker image to a regular file:

```
sudo docker save -o stompbroker.docker-image kisp/stompbroker
```

## TO load a docker image from a file:

```
docker load -i <path>
```
