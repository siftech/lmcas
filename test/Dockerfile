FROM alpine@sha256:4f83fc905e4551f4e07de298f55745e766cc880f890244c1e6872aab60f0a0f6

# For proot
RUN echo "https://dl-cdn.alpinelinux.org/alpine/edge/testing" >> "/etc/apk/repositories"

RUN apk update
RUN apk upgrade
RUN apk add bash curl dbus openssh proot py3-pyroute2 py3-pytest py3-pip py3-psutil strace
RUN pip install --break-system-packages multimethod paramiko pytest-json-report requests toml
