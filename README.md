# MisraCPP Project - Demo

### Installation
```bash
git clone https://github.com/llvm-mirror/llvm.git -b release_70 --depth=1 llvm
git clone https://github.com/llvm-mirror/clang.git -b release_70 --depth=1 llvm/tools/clang
git clone https://github.com/nthu-pllab/MisraCPPDemo.git llvm/tools/clang/MisraCPP

echo "add_clang_subdirectory(MisraCPP)" >> llvm/tools/clang/CMakeLists.txt

cmake -S llvm -B build \
  -GNinja \
  -DLLVM_TARGETS_TO_BUILD=X86 \

cmake --build build

mkdir llvm/tools/clang/MisraCPP/tools/misra-scan/lib
mkdir llvm/tools/clang/MisraCPP/tools/misra-scan/bin
cp build/bin/clang build/bin/scan-build llvm/tools/clang/MisraCPP/tools/misra-scan/bin/
cp build/lib/MisracppChecker.so         llvm/tools/clang/MisraCPP/tools/misra-scan/lib/
cp -r build/lib/clang                   llvm/tools/clang/MisraCPP/tools/misra-scan/lib/

python3 -m venv MisraCpp-venv
source MisraCpp-venv/bin/activate
pip3 install -r llvm/tools/clang/MisraCPP/tools/misra-scan/requirements.txt
```

### Running examples
```bash
cd llvm/tools/clang/MisraCPP/example

cd check-single-file
./misra-run.sh 14_8_2.cpp

cd ../check-project
bash check-libzmq.sh
cd report/{TIMESTAMP}
# open the index.html in browser
# or set up a http server, e.g. `python3 -m http.server 8000`
# then open {server-ip-address}:8000 on your host browser
```
