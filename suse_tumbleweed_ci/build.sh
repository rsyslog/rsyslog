rm -rf common
cp -r ../common common
#sudo docker build --no-cache -t rg .
sudo docker build --no-cache --rm -t suse_tumbleweed_ci .
rm -rf common
