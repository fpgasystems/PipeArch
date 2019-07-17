# Use 1 instance not to share bandwidth

# FCFS
./cs_overhead 18 --er 131072 32 32 20
./cs_overhead 18 --er 131072 64 32 20
./cs_overhead 18 --er 131072 128 32 20
./cs_overhead 18 --er 131072 256 32 20
./cs_overhead 18 --er 131072 512 32 20
./cs_overhead 18 --er 131072 1024 32 20

# High Priority (SJF)
./cs_overhead 18 e-er 131072 32 32 20
./cs_overhead 18 e-er 131072 64 32 20
./cs_overhead 18 e-er 131072 128 32 20
./cs_overhead 18 e-er 131072 256 32 20
./cs_overhead 18 e-er 131072 512 32 20 
./cs_overhead 18 e-er 131072 1024 32 20
