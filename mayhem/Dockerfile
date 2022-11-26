FROM --platform=linux/amd64 ubuntu:22.04 as builder

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential cmake freeglut3-dev clang

COPY . /repo
WORKDIR /repo
RUN ln -s /usr/bin/gcc /usr/bin/gcc-9
RUN ln -s /usr/bin/g++ /usr/bin/g++-9
RUN chmod +x ./generate_gcc_linux_min.sh
RUN ./generate_gcc_linux_min.sh
WORKDIR /repo/cmake_temp
RUN make -j8
RUN clang++ /repo/mayhem/fuzz_isUtf8Text.cpp -fsanitize=fuzzer,address -L /repo/cmake_temp/ -llibDaScript -o /fuzz

FROM ubuntu:22.04 as package
COPY --from=builder /repo/bin/daScript /
COPY --from=builder /fuzz /fuzz_isUtf8Text
