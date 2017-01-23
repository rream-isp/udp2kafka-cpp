CLION_VERSION=2016.3.3
CLANG_VERSION=3.8
CONFLUENT_PLATFORM_VERSION=2.11

echo -----------------------------------
echo INSTALL GIT
echo -----------------------------------
apt-get -y install git

echo -----------------------------------
echo INSTALL CLANG
echo -----------------------------------
for PROG in clang lldb; do
	apt-get -y install ${PROG}-${CLANG_VERSION}
	for C in /usr/bin/${PROG}*${CLANG_VERSION}; do
		L=${C%-$CLANG_VERSION}
		B=$(basename $L)
		update-alternatives --install $L $B $C 1000
	done
done
update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100 && \
update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100

echo -----------------------------------
echo INSTALL CMAKE
echo -----------------------------------
apt-get -y install cmake

echo -----------------------------------
echo INSTALL LIBRARIES
echo -----------------------------------
wget -qO - http://packages.confluent.io/deb/3.0/archive.key | apt-key add - && \
add-apt-repository "deb [arch=amd64] http://packages.confluent.io/deb/3.0 stable main" && \
apt-get update && \
apt-get install confluent-platform-${CONFLUENT_PLATFORM_VERSION} && \
apt-get -y install librdkafka1 librdkafka-dev librdkafka1-dbg librdkafka++1 && \
apt-get -y install libboost-all-dev

echo -----------------------------------
echo DOWNLOAD CLION
echo -----------------------------------
[! -e /opt/clion-2016.3.1/bin/clion.sh ] && \
rm CLion-${CLION_VERSION}.tar.* && \
wget -q http://download.jetbrains.com/cpp/CLion-${CLION_VERSION}.tar.gz && \
tar xfz CLion-${CLION_VERSION}.tar.gz -C /opt && \
rm CLion-${CLION_VERSION}.tar.* && \
cd /opt/clion-${CLION_VERSION}/bin/ && \
./clion.sh &