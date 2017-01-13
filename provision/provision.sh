CLION_VERSION=2016.3.1
CLANG_VERSION=3.8

sudo apt-get update && apt-get -y upgrade && apt-get -y autoremove	

echo -----------------------------------
echo INSTALL GIT
echo -----------------------------------
sudo apt-get -y install git

echo -----------------------------------
echo INSTALL COMPILER
echo -----------------------------------
for PROG in clang lldb; do
	sudo apt-get -y install ${PROG}-${CLANG_VERSION}
	for C in /usr/bin/${PROG}*${CLANG_VERSION}; do
		L=${C%-$CLANG_VERSION}
		B=$(basename $L)
		sudo update-alternatives --install $L $B $C 1000
	done
done

sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100

echo -----------------------------------
echo INSTALL CMAKE
echo -----------------------------------
sudo apt-get -y install cmake

echo -----------------------------------
echo DOWNLOAD CLION
echo -----------------------------------

sudo wget -q http://download.jetbrains.com/cpp/CLion-${CLION_VERSION}.tar.gz
sudo tar xfz CLion-${CLION_VERSION}.tar.gz -C /opt

echo -----------------------------------
echo INSTALL CONAN
echo -----------------------------------
sudo apt-get -y install python-pip
sudo pip install --upgrade pip
sudo pip install conan
su -l -c conan vagrant
sed -i "s/compiler=gcc/compiler=clang/; s/version=[0-9.]*/version=$CLANG_VERSION/; s/libstdc++/libstdc++11/" ~vagrant/.conan/conan.conf

