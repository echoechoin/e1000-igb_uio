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

### 0. 编译

```bash
make
```

### 1. 报文接收

```bash
./e1000-test -i <网卡二PCI ID> -m recv
```

然后新建一个终端，运行测试程序：

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

### 2. 报文发送

```bash
./e1000-test -i <网卡二PCI ID> -m send
```

然后新建一个终端，监听网卡3上的数据包：

```bash
tcpdump -i <网卡3>
```

可以看到网卡三收到网卡二发送过来的报文：

```
...

02:49:03.636248 ARP, Ethernet (len 6), IPv4 (len 4), Request who-has 1.2.3.4 tell 1.2.3.4, length 46
02:49:04.637978 ARP, Ethernet (len 6), IPv4 (len 4), Request who-has 1.2.3.4 tell 1.2.3.4, length 46
02:49:05.639219 ARP, Ethernet (len 6), IPv4 (len 4), Request who-has 1.2.3.4 tell 1.2.3.4, length 46

...
```

### 3. 中断处理

由于[VMWARE 环境下 82545EM 虚拟网卡不支持 msix、intx 中断](https://blog.csdn.net/Longyu_wlz/article/details/121443906)，暂时无法测试中断处理。