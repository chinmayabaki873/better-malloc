FROM ubuntu:22.04

RUN apt-get update && apt-get install -y gcc make python3 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

# Compile libmymalloc.so and test_main inside the Linux container
RUN make clean && make

EXPOSE 8080

CMD ["python3", "server.py"]