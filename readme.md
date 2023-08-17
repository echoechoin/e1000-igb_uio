# e1000 用户态驱动实现

测试的网卡是VMware默认的e1000网卡：
- Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) (rev 01)

## 准备工作

创建一个虚拟机，添加三个e1000网卡，其中网卡1用来连ssh，网卡2和网卡3放在同一个虚拟网络上。

下载编译igb_uio驱动，然后加载驱动：

```bash
git clone git://dpdk.org/dpdk-kmods
cd dpdk-kmods/linux/igb_uio && make
modprobe uio
insmod ./igb_uio.ko
```

将网卡2绑定到igb_uio驱动上：

```
./utils/dpdk-devbind.py --bind=igb_uio <网卡2的pci_id>
```

## 编译和测试

```bash
make
./e1000-test # 进入驱动程序
```

新建一个终端，运行测试程序：

```bash
ip link set <网卡3> up
tcpreply -i <网卡3> ./test/test.pcap
```

此时，测试程序会收到网卡3上的数据包，并打印出来。

```
...
receive desc:        18
receive total:       371
receive packet src:  00:50:56:c0:00:0a
               dest: 01:00:5e:7f:ff:fa
receive desc:        19
receive total:       372
receive packet src:  00:50:56:c0:00:0a
               dest: 01:00:5e:7f:ff:fa
receive desc:        20
receive total:       373
receive packet src:  00:50:56:c0:00:0a
               dest: 01:00:5e:7f:ff:fa
...
```

