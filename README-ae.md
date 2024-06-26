# YCSB-cpp for Artifact Evaluation

## ❗️For AE Reviewers
The cluster is now ready. However, Cloudlab only permits me to use it till Oct 20. Please try to finish the experiments before that day. Thanks!

## Architecture
The evaluation cluster consists of 8 machines:

- 1 Client machine: run YCSB benchmark
- 1 App server machine: run database applications
- 3 Ceph machine: run CephFS
- 3 NCL peers: host replication of NCL-backed files

The CephFS is mounted to the app server. Client use eRPC or TCP (depend on application implementation) to send commands to app server. App server use RDMA to backup NCL-backed files to NCL peers.
```
                                           ┌────────────┐
                                     ┌─────► NCL Peer 0 │
                                     │     └────────────┘
┌──────────┐          ┌───────────┐  │
│          │          │           │  │     ┌────────────┐
│  client  ├─eRPC/TCP─► app server├─RDMA───► NCL Peer 1 │
│          │          │           │  │     └────────────┘
└──────────┘          └─────┬─────┘  │
                            │        │     ┌────────────┐
                            │        └─────► NCL Peer 2 │
                            │              └────────────┘
                            │
           ┌────────────────▼────────────────┐
           │  cephfs cluster                 │
           │                                 │
           │  ┌───────┐ ┌───────┐ ┌───────┐  │
           │  │ rep 0 │ │ rep 1 │ │ rep 2 │  │
           │  └───────┘ └───────┘ └───────┘  │
           │                                 │
           └─────────────────────────────────┘
```

## Download
Download to app server machine
```bash
# On app server
git clone https://github.com/dassl-uiuc/YCSB-cpp.git --recurse-submodules
```
Then, for convenience, mount the folder to client machine via NFS
```bash
# On client
sudo mount.nfs4 ${server_ip}:/data/YCSB-cpp /data/YCSB-cpp
```

## Install Dependency Packages

### Regular dependencies
Install on client machine
```bash
sudo apt-get install -y libhiredis-dev cmake python3-pip

git clone https://github.com/sewenew/redis-plus-plus.git redis++
cd redis++
mkdir build
cd build
cmake ..
make -j
sudo make install
cd ..

sudo pip3 install matplotlib
```

Install on app server machine
```bash
cd YCSB-cpp
./install-dep.sh ${DIR_TO_INSTALL}
```

### NCL Library
Build it on the app server node and mount the NCL folder to each replication peer. See https://github.com/dassl-uiuc/compute-side-log for instructions.

### eRPC
On both client and app server, download and compile
```bash
git clone https://github.com/erpc-io/eRPC.git
cd eRPC
cmake . -DPERF=ON -DTRANSPORT=infiniband -DROCE=ON -DLOG_LEVEL=info
make -j
```
Configure huge pages
```bash
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages
```
To automatic configure on boot, edit `/etc/sysctl.conf`
```
vm.nr_hugepages = 1024
```
See https://help.ubuntu.com/community/KVM%20-%20Using%20Hugepages for detail.

## Install Applications
Install on the App server machine

### Install RocksDB
```bash
git clone https://github.com/dassl-uiuc/rocksdb.git
cd rocksdb
git checkout csl-dev
make shared_lib -j
```
See https://github.com/dassl-uiuc/rocksdb/blob/main/INSTALL.md for detail.

### Install Redis
```bash
git clone https://github.com/dassl-uiuc/redis.git
cd redis
git checkout ncl-dev
make -j
```

### Install SQLite
```bash
git clone https://github.com/dassl-uiuc/sqlite.git
cd sqlite
git checkout ncl-dev
mkdir build
cd build
../configure
make -j
```

## Run Experiments

### Node list
- app server (node-0): `ssh luoxh@hp069.utah.cloudlab.us`
- memory replicas
  - node-1: `ssh luoxh@hp075.utah.cloudlab.us`
  - node-2: `ssh luoxh@hp055.utah.cloudlab.us`
  - node-3: `ssh luoxh@hp076.utah.cloudlab.us`
- client (node-4): `ssh luoxh@hp057.utah.cloudlab.us`
- Ceph nodes
  - node-5: `ssh luoxh@hp065.utah.cloudlab.us`
  - node-6: `ssh luoxh@hp074.utah.cloudlab.us`
  - node-7: `ssh luoxh@hp066.utah.cloudlab.us`

### (C1) Write Microbenchmark (🟢 ready)
On the **app server node** (node-0), run
```bash
cd /data/YCSB-cpp
./scripts/run_raw_wr.sh
mkdir -p /data/result/fig
python3 ./scripts/draw/raw_wr.py /data/result
```
Generated figure will be at `/data/result/fig/write_lat.pdf` on **app server node**

### (C2) Insert-Only Workload (🟢 ready)
Configure before running
- RocksDB
```
# configure client and server address in rocksdb-clisvr/rocksdb-clisvr.properties
rocksdb-clisvr.client_hostname=localhost
rocksdb-clisvr.server_hostname=localhost
```
- Redis
```
# configure server address in redis/redis.properties
redis.host=localhost
```
- SQLite
```
# configure client and server address in sqlite/sqlite.properties
sqlite.client_hostname=localhost
sqlite.server_hostname=localhost
```
You may use one script to run all 3 applications on the **client node** (node-4)
```bash
./scripts/run_all.sh load
```
Or you can run each application separately in the following steps:

#### RocksDB (Estimated runtime within 2.5h)
Build on **server node** (node-0)
```bash
cd /data/YCSB-cpp
rm ycsb rocksdb_svr
make BIND_ROCKSDBCLI=1 EXTRA_CXXFLAGS="-I/data/eRPC/src -I/data/eRPC/third_party/asio/include -I/data/rocksdb/include -L/data/eRPC/build -L/data/rocksdb" -j
```
Run on **client node** (node-4).
```bash
cd /data/YCSB-cpp
./scripts/run_rocksdb.sh cephfs load
./scripts/run_rocksdb.sh sync load
./scripts/run_rocksdb.sh sync_ncl load
# Each of the script runs for 50min
mkdir -p /data/result/fig
python3 ./scripts/draw/insert_only.py /data/result
```
Generated figure will be at `/data/result/fig/rocksdb_lattput.pdf` on **client node**

#### Redis (Estimated runtime within 1.5h)
Build on **server node** (node-0)
```bash
cd /data/YCSB-cpp
rm ycsb
make BIND_REDIS=1 -j
```
Run on **client node** (node-4)
```bash
cd /data/YCSB-cpp
./scripts/run_redis.sh cephfs load
./scripts/run_redis.sh sync load
./scripts/run_redis.sh sync_ncl load
# Each of the script runs for 30min
mkdir -p /data/result/fig
python3 ./scripts/draw/insert_only.py /data/result
```
Generated figure will be at `/data/result/fig/redis_lattput.pdf` on **client node**

#### SQLite (Estimated runtime within 20min)
Build on **server node** (node-0)
```bash
make BIND_SQLITE=1 EXTRA_CXXFLAGS="-I/data/eRPC/src -I/data/eRPC/third_party/asio/include -I/data/sqlite/build -L/data/eRPC/build" -j
```
Run on **client node** (node-4)
```bash
cd /data/YCSB-cpp
./scripts/run_sqlite.sh cephfs load
./scripts/run_sqlite.sh sync load
./scripts/run_sqlite.sh sync_ncl load

mkdir -p /data/result/fig
python3 ./scripts/draw/insert_only.py /data/result
```
Generated figure will be at `/data/result/fig/sqlite_lattput.pdf` on **client node**
### (C3) YCSB Workload (🟢 ready)
#### Generating Base DB
We need a base DB to run the YCSB workloads. For RocksDB and Redis, it's a DB that contains 100M records, for SQLite, it contains 10M records.

Run this script to generate base DBs (estimated time within 1h).
```bash
./scripts/gen_base_db.sh
```

You may use one script to run all 3 applications on the **client node** (node-4)
```bash
./scripts/run_all.sh run
```
Or you can run each application separately in the following steps:

#### RocksDB (Estimated runtime within 2h)
Build on **server node** (node-0), then run on **client node** (node-4)
```bash
cd /data/YCSB-cpp
./scripts/run_rocksdb.sh cephfs run
./scripts/run_rocksdb.sh sync run
./scripts/run_rocksdb.sh sync_ncl run
# Each of the script runs for 40min
mkdir -p /data/result/fig
python3 ./scripts/draw/ycsb.py /data/result
```
Generated figure will be at `/data/result/fig/rocksdb_ycsb.pdf` on **client node**

#### Redis (Estimated runtime within 2h)
Build on **server node** (node-0), then run on **client node** (node-4)
```bash
cd /data/YCSB-cpp
./scripts/run_redis.sh cephfs run
./scripts/run_redis.sh sync run
./scripts/run_redis.sh sync_ncl run
# Each of the script runs for 40min
mkdir -p /data/result/fig
python3 ./scripts/draw/ycsb.py /data/result
```
Generated figure will be at `/data/result/fig/redis_ycsb.pdf` on **client node**

#### SQLite (Estimated runtime within 1.5h)
Build on **server node** (node-0), then run on **client node** (node-4)
```bash
cd /data/YCSB-cpp
./scripts/run_sqlite.sh cephfs run
./scripts/run_sqlite.sh sync run
./scripts/run_sqlite.sh sync_ncl run
# Each of the script runs for 30min
mkdir -p /data/result/fig
python3 ./scripts/draw/ycsb.py /data/result
```
Generated figure will be at `/data/result/fig/sqlite_ycsb.pdf` on **client node**

### (C4) Recovery Benchmark (🟢 ready)

#### Read Microbenchmark (🟢 ready)
On the **app server node** (node-0), run
```bash
cd /data/YCSB-cpp
./scripts/run_raw_rd.sh
mkdir -p /data/result/fig
python3 ./scripts/draw/raw_rd.py /data/result
```
Generated figure will be at `/data/result/fig/read_lat.pdf` on **app server node**

#### Application Recovery (🟢 ready)
On the **app server node** (node-0), run
```bash
cd /data/YCSB-cpp
./scripts/run_app_recover.sh
mkdir -p /data/result/fig
python3 ./scripts/draw/recovery.py /data/result
```
Generated figure will be at `/data/result/fig/recovery-time.pdf` on **app server node**
