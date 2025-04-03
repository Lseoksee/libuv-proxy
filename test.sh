apt update && apt upgrade -y && apt install gcc g++ make cmake jq pkg-config -y
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
bash ~/vcpkg/bootstrap-vcpkg.sh
 ~/vcpkg/vcpkg integrate install

git clone https://github.com/Lseoksee/libuv-proxy.git ~/libuv-proxy
cd ~/libuv-proxy

cp /usr/bin/arm-linux-gnueabihf-gcc /usr/bin/gcc
cp /usr/bin/arm-linux-gnueabihf-g++ /usr/bin/g++


jq '.configurePresets[0].environment.CMAKE_ARCH = "${{env.VCPKG_PATH}}/scripts/buildsystems/vcpkg.cmake"' < CMakePresets.json > temp.json
mv temp.json CMakePresets.json
jq '.configurePresets[0].environment.VCPKG_ARCH = "${{env.VCPKG_PATH}}/scripts/buildsystems/vcpkg.cmake"' < CMakePresets.json > temp.json
mv temp.json CMakePresets.json
jq '.configurePresets[0].environment.VCPKG_TOOLCHAIN_FILE = "${{env.VCPKG_PATH}}/scripts/buildsystems/vcpkg.cmake"' < CMakePresets.json > temp.json
mv temp.json CMakePresets.json
jq '.configurePresets[0].environment.CROSS_ONLY_FIND_ROOT_PATH = "${{env.VCPKG_PATH}}/scripts/buildsystems/vcpkg.cmake"' < CMakePresets.json > temp.json
mv temp.json CMakePresets.json

cmake --preset Release-linux
cmake --build --preset Release-linux
# ~/vcpkg/scripts/buildsystems/vcpkg.cmake


윈도우

64비트
x86_64-w64-mingw32-gcc
x86_64-w64-mingw32-g++

32비트
i686-w64-mingw32-gcc
i686-w64-mingw32-g++

리눅스
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

arm
arm-linux-gnueabihf-gcc
arm-linux-gnueabihf-g++

aarch64
aarch64-linux-gnu-gcc


