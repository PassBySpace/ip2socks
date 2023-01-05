# ip2socks start
ip2socks --config=../scripts/config.darwin.example.yml

# home debug

sudo ifconfig utun3 10.10.0.2 netmask 255.255.255.0 10.10.0.1 up
sudo ifconfig utun3 up

sudo route add 1.1.1.1 -link utun4

sudo route add 1.1.1.1 10.0.0.1
sudo route delete 1.1.1.1

sudo route add -net 10.0.0.0 -netmask 255.0.0.0 -interface utun3

curl 10.0.0.2

curl www.baidu.com

sudo route delete -net 10.0.0.0 -netmask 255.0.0.0 -interface utun3
sudo route add -net 10.0.0.0 -netmask 255.0.0.0 -interface utun3

sudo route add 10.0.0.0

sudo route add www.baidu.com 10.0.0.1

sudo route add 10 -netmask 255.0.0.0 -interface utun3

curl http://www.baidu.com --resolve www.baidu.com:80:10.0.0.2

sudo route add 198.18.6.153 -netmask 255.0.0.0 -interface utun3

sudo route add 198.18.6.153 -interface utun3

sudo route add 93.184.216.34 -interface utun3

curl http://www.example.com

# work debug

sudo route add 10.0.0.2 -interface utun4

# start flow
# 1. start simple web server (use node-express or rust-warp)
# 2. start golang socks5 server
# 3. start ip2socks, mac utun -> socks
# 4. add route table 10.0.0.2 -> utun4
# 5. test use curl 10.0.0.2
