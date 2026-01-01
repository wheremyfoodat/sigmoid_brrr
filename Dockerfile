# Build image with docker build --tag my_image .
# Create a container with docker run -it my_image
# Start the container on subsequent runs with docker start
# Inside the container is a setup.sh file for setting up the repo
FROM ubuntu:24.04
SHELL ["/bin/bash", "-c"]
CMD ["/bin/bash"]

RUN apt update && apt dist-upgrade -y

# Install build tools and X11 libraries
# Install sudo too, just so sudo commands don't fail in the container
RUN apt install -y sudo git make pkg-config cmake autoconf flex bison libfl-dev help2man
RUN apt install -y build-essential python3-full python3-pip python-is-python3 libx11-dev libxext-dev libgl1 libglx-mesa0 mesa-common-dev libwayland-dev libgl1-mesa-dev

# Build latest verilator from source, to make sure it's new enough
RUN git clone https://github.com/verilator/verilator && cd verilator && autoconf && ./configure && make -j `nproc` && sudo make install && cd ..

# Add a setup.sh to the container that clones the repo and builds agbcc+tools
RUN echo "git clone https://github.com/wheremyfoodat/sigmoid_brrr" >> setup.sh && \
    chmod 777 setup.sh