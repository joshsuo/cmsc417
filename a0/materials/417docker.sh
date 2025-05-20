#!/bin/bash

if [ `id -u` -eq 0 ]; then
    echo "Do not run this script as root. This is necessary to detect your uid/gid. `sudo` is prepended to Docker commands already."
    exit 1
fi

if [ `uname -s` = "Linux" ]
then
	uid=`id -u`
	gid=`id -g`
elif [ `uname -s` = "Darwin" ]
then
	uid=1000
	gid=1000
else
	echo "Are you using BSD, Solaris, or some other bizarro UNIX? Please email the TAs, we're curious."
	exit 1
fi

scriptDir="$(pwd)"
dockerfilePath="."
imageName="cmsc417spr24image"
imageTag="latest"
container="cmsc417container"

vscode=0

mkdir -p CMSC417
touch CMSC417/.bashrc
wd="$(pwd)/CMSC417"

while getopts 'v' flag; do
  case "${flag}" in
    v) vscode=1 ;;
    *) error "Unexpected option ${flag}" ;;
  esac
done

if [ $vscode -eq 1 ]
then
    mkdir -p .devcontainer
    cd .devcontainer

cat >devcontainer.json <<EOL
{
    "name": "$container",
    "image": "$imageName:$imageTag",
    "workspaceFolder": "/home/student",
    "workspaceMount": "source=$scriptDir/CMSC417,target=/home/student,type=bind",
    "customizations": {
      "vscode": {
        "settings": {
            "terminal.integrated.defaultProfile.linux": "bash",
            "editor.tabSize": 4
        },
        "extensions": [
          "jeff-hykin.better-cpp-syntax",
          "ms-vscode.cpptools",
          "ms-vscode.cpptools-extension-pack",
          "ms-vscode.cpptools-themes",
          "eamodio.gitlens",
          "aaron-bond.better-comments"
        ]
      }
    },
    "runArgs": ["--name", "$container"]
}

EOL

cd ..

fi

# if on Apple Silicon and something breaks, try ARM64
cat >Dockerfile <<EOL
FROM --platform=linux/amd64 debian:12

LABEL description="CMSC417 Docker Image"

ENV TZ="America/New_York"

RUN apt-get -yq update && \
    apt-get -yq upgrade && \
    apt-get -yq install \
    git \
    make \
    gcc \
    g++ \
    emacs-nox \
    vim \
    sudo \
    libssl-dev \
    libncurses-dev \
    libev-dev \
    cmake \
    protobuf-compiler \
    htop \
    curl \
    bison \
    flex \ 
    build-essential \
    valgrind && \
    ln -fs /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN groupadd -g $gid student && \
    useradd -r -u $uid -g student student && \
    usermod --shell /bin/bash student && \
    echo 'student ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers && \
    mkdir /home/student && \
    chown student:student /home/student

USER student:student
RUN id -u -n && \
    cd && curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

EOL
 
# if you're on a Mac and the below breaks, try making it "DOCKER_BUILDKIT=0 sudo -E docker build"
# https://stackoverflow.com/a/66695181
if [ "$(sudo docker image inspect $imageName:$imageTag 2> /dev/null)" = "[]" ]; then
	echo "CMSC417 docker image does not exist, building it"
	sudo docker build -t $imageName:$imageTag .
    if [ "$(sudo docker image inspect $imageName:$imageTag 2> /dev/null)" = "[]" ]; then
        echo "Build failed"
        exit -1
    fi
fi

if [ $vscode -eq 1 ]
then
    echo "If not already done, open this directory $(pwd) in your VSCode - just as with \`code $(pwd)\` It should prompt you to open a docker container."
    echo "If not, you should install https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers"
    echo "Optionally, you can install https://marketplace.visualstudio.com/items?itemName=ms-azuretools.vscode-docker"
    echo
    echo "It may be beneficial to append the following lines to your VSCode User Setttings"
    echo "\"remote.containers.defaultExtensions\": ["
    echo "  \"ms-vscode.cpptools\","
    echo "  \"ms-vscode.cpptools-extension-pack\","
    echo "  \"jeff-hykin.better-cpp-syntax\","
    echo "  \"aaron-bond.better-comments\""
    echo "],"
    echo "\"terminal.integrated.defaultProfile.linux\": \"bash\""

    exit 0
fi

if [ "$(sudo docker container inspect $container 2> /dev/null)" = "[]" ]; then
    echo "Container does not exist, creating it"
    sudo docker create --name=$container -v "$wd":/home/student -ti $imageName:$imageTag
    if [ "$(sudo docker container inspect $container 2> /dev/null)" = "[]" ]; then
        echo "Could not create container"
        exit 1
    fi
fi

if [ "$(sudo docker container inspect $container -f '{{.State.Status}}' 2> /dev/null)" != "running" ]; then
    echo "Container not running, starting it"
    sudo docker start $container
    if [ "$(sudo docker container inspect $container -f '{{.State.Status}}' 2> /dev/null)" != "running" ]; then
        echo "Could not start container"
        exit 2
    fi
fi

sudo docker exec -u $uid -it $container /bin/bash -c 'cd; bash'
