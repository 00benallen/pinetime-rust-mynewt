#!/usr/bin/env bash
#  Install Rust and Mynewt Build Tools for Raspberry Pi.  Based on https://mynewt.apache.org/latest/newt/install/newt_linux.html.  

echo "Installing Rust and Mynewt Build Tools for Raspberry Pi..."
set -e  #  Exit when any command fails.
set -x  #  Echo all commands.

#  Preqrequisites: (Remove folders ~/pinetime-rust-mynewt and ~/openocd-spi if they exist)
#  sudo apt install -y wget p7zip-full
#  cd ~
#  wget https://github.com/lupyuen/pinetime-rust-mynewt/releases/download/v2.0.2/pinetime-rust-mynewt.7z
#  7z x pinetime-rust-mynewt.7z
#  rm pinetime-rust-mynewt.7z
#  cd ~/pinetime-rust-mynewt
#  scripts/install-pi.sh

#  Configure scripts for Raspberry Pi
echo "source scripts/nrf52-pi/config.sh" >scripts/config.sh

set +x; echo; echo "----- Setting versions..."; set -x
source scripts/install-version.sh

set +x; echo; echo "----- Installing Rust. When prompted, press Enter to select default option..."; sleep 5; set -x
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
rustup default nightly
rustup update
rustup target add thumbv7em-none-eabihf

set +x; echo; echo "----- Installing build tools..."; set -x
sudo apt install -y wget git autoconf libtool make pkg-config libusb-1.0-0 libusb-1.0-0-dev libhidapi-dev libftdi-dev telnet p7zip-full

set +x; echo; echo "----- Installing gcc..."; set -x 
sudo apt install -y gcc-arm-none-eabi

set +x; echo; echo "----- Installing gdb..."; set -x 
if [ ! -e /usr/bin/arm-none-eabi-gdb ]; then
    sudo apt install -y gdb-multiarch
    sudo ln -s /usr/bin/gdb-multiarch /usr/bin/arm-none-eabi-gdb
fi

set +x; echo; echo "----- Installing VSCode..."; set -x 
if [ ! -e /usr/bin/code-oss ]; then
    cd ~
    wget https://code.headmelted.com/installers/apt.sh 
    chmod +x apt.sh
    sudo -s ./apt.sh
fi

set +x; echo; echo "----- Installing openocd-spi..."; set -x 
if [ ! -d $HOME/openocd-spi ]; then
    pushd $HOME
    git clone --branch $openocd_version https://github.com/lupyuen/openocd-spi
    cd openocd-spi
    ./bootstrap
    ./configure --enable-sysfsgpio --enable-bcm2835spi --enable-cmsis-dap
    make
    popd
fi
cp $HOME/openocd-spi/src/openocd $HOME/pinetime-rust-mynewt/openocd/bin/openocd

#  Install go for building newt
set +x; echo; echo "----- Installing go..."; set -x 
golangpath=/usr/lib/go-1.13.6/bin
if [ ! -e $golangpath/go ]; then
    wget https://dl.google.com/go/go1.13.6.linux-armv6l.tar.gz
    tar xf go*.tar.gz
    sudo mv go /usr/lib/go-1.13.6
    rm go*.tar.gz

    echo export PATH=$golangpath:\$PATH >> ~/.bashrc
    echo export PATH=$golangpath:\$PATH >> ~/.profile
    echo export GOROOT= >> ~/.bashrc
    echo export GOROOT= >> ~/.profile
fi
export PATH=$golangpath:$PATH
#  Prevent mismatch library errors when building newt.
export GOROOT=
go version  #  Should show "go1.13" or later.

#  Change owner from root back to user for the installed packages.
set +x; echo; echo "----- Fixing ownership..."; set -x 
if [ -d "$HOME/.caches" ]; then
    sudo chown -R $USER:$USER "$HOME/.caches"
fi
if [ -d "$HOME/.config" ]; then
    sudo chown -R $USER:$USER "$HOME/.config"
fi
if [ -d "$HOME/opt" ]; then
    sudo chown -R $USER:$USER "$HOME/opt"
fi
set +x; echo; set -x

#  Build newt in /tmp/mynewt. Copy to /usr/local/bin.
set +x; echo; echo "----- Installing newt..."; set -x 
if [ ! -e /usr/local/bin/newt ]; then
    mynewtpath=/tmp/mynewt
    if [ -d $mynewtpath ]; then
        rm -rf $mynewtpath
    fi
    mkdir $mynewtpath
    pushd $mynewtpath

    git clone --branch $mynewt_version https://github.com/apache/mynewt-newt/
    cd mynewt-newt/
    ./build.sh
    sudo mv newt/newt /usr/local/bin
    popd
fi
newt version  #  Should show "Version: 1.7.0" or later.  Should NOT show "...-dev".

#  echo "----- Installing mynewt..."
#  Remove the existing Mynewt OS in "repos"
#  if [ -d repos ]; then
#      rm -rf repos
#  fi
#  Download Mynewt OS into the current project folder, under "repos" subfolder.
#  newt install -v -f

set +x  #  Stop echoing all commands.
echo ✅ ◾ ️Done!
