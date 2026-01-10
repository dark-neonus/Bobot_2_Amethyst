ARG DOCKER_TAG=latest
FROM espressif/idf:${DOCKER_TAG}

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Set timezone
ENV TZ=Europe/Amsterdam
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install dependencies
RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get install -y \
    udev \
    git \
    unzip \
    curl \
    build-essential \
    cmake \
    ninja-build \
    libx11-dev \
    libxcursor-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libfontconfig1-dev

RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

# Copy and setup compile script
COPY compile.sh /compile.sh
RUN chmod +x /compile.sh

# Set up volumes
VOLUME /dependencies
VOLUME /output

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]

CMD ["/bin/bash", "-c"]