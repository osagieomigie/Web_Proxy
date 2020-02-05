echo -e "Starting proxy"

rm proxy 
g++ proxy.cpp -o proxy

echo "Running proxy on port 8080"
 
./proxy 8080
