FROM gitpod/workspace-full

RUN sudo apt-get update \
    && sudo apt-get install -y \
        libvulkan-dev \
        mesa-vulkan-drivers \
        vulkan-utils \
    && sudo rm -rf /var/lib/apt/lists/*
