#include<linux/module.h>
#include<linux/sched.h>
#include<linux/kernel.h>
#include<linux/slab.h>
#include<linux/errno.h>
#include<linux/types.h>
#include<linux/interrupt.h>
#include<linux/in.h>
#include<linux/netdevice.h>
#include<linux/etherdevice.h>
#include<linux/ip.h>
#include<linux/tcp.h>
#include<linux/skbuff.h>
#include<linux/if_ether.h>
#include<linux/in6.h>
#include<asm/checksum.h>
#include<linux/platform_device.h>
#include<linux/timer.h>
//#include<linux/io.h>

//#include <linux/uaccess.h>
#include <asm/io.h>


#define  MAC_AUTO
static struct net_device *vir_net_devs;

static void sram_timer_expires(struct timer_list *t);

static struct timer_list sram_timer;

void vir_net_rx(struct net_device *pdev, u16 len, unsigned char *buf);

#define DEFINE_TIMER(sram_timer,sram_timer_expires);

#define SRAM_BASE_ADDRESS   (0x34000000)

#define SRAM_DDR_HEAD       (0x0)    ///address
#define SRAM_DDR_TAIL       (0x4)     /// address

#define VNET_UP_DOWN        (0x8)     ///address
#define VNET_UP             (0x55)   /// UP
#define VNET_DOWN           (0xAA)   /// DOWN

#define SRAM_MES_ADDRESS    (0xE0)


#define DDR_BASE_ADDRESS    (0x88000000)

#define TIMER_VALUE        50        //// timer value


static void __iomem *sram_base;
static void __iomem *ddr_base;
static void __iomem *ppos;



static void sram_timer_expires(struct timer_list *t)
{
   u32 sramdrr_head,sramddr_tail;

//   printk("sram read test \r\n");

   sramdrr_head = readl(sram_base+SRAM_DDR_HEAD);
   sramddr_tail = readl(sram_base+SRAM_DDR_TAIL);


   bool error = false;

   if(sramdrr_head != sramddr_tail)
   {

      printk(KERN_INFO "sram_head = %x\n", sramdrr_head);
      printk(KERN_INFO "sram_tail = %x\n", sramddr_tail);

      u32 off = sramdrr_head - DDR_BASE_ADDRESS;

      printk(KERN_INFO "one offset value[0]= %x\n",off);

      if(off < 0x4000000 )
      {
        u16 len =0;

        len=readb(ddr_base+off);
        len=len+256*(u16)readb(ddr_base+off+1);
 
        printk(KERN_INFO "ddrdata len value[0]= %x\n",len);

        if( len < 0x1024)
        {
           unsigned char *datvalue = (unsigned char *)kmalloc(len,GFP_ATOMIC); 

           u16 i;

          // for(i=0;i<len;i++)
          //    *(datvalue+i) = readb(ddr_base+off+2+i);

           memcpy(datvalue,(ddr_base+off+2),len);
           
           for(i=0;i<len;i++)
             printk(KERN_INFO "ddrdata value= %x\n",*(datvalue+i));

           vir_net_rx(vir_net_devs,len,datvalue); 
           
           kfree(datvalue);

           u32 next = sramdrr_head+len+2;

           printk(KERN_INFO "next point = %x\n",next);

           if( next < DDR_BASE_ADDRESS || next > DDR_BASE_ADDRESS+0x4000000)
           {
              error = true;
              printk(KERN_INFO "next point address error %x\n");
              
           }
           else
           {
              
               writel(next,sram_base+SRAM_DDR_HEAD);
           }
        }
        else
        {
           error = true;
           printk(KERN_INFO "data length error ! %x\n");
        }
     }
     else
     {
        error = true;
        printk(KERN_INFO "ddr offset length error ! %x\n");
     }
   }
   
   if( error == false)
       mod_timer(&sram_timer,jiffies + msecs_to_jiffies(TIMER_VALUE));
}


int vir_net_open(struct net_device *dev) {
#ifndef MAC_AUTO
    int i;
    for (i=0; i<6; i++) {
        dev->dev_addr[i] = 0xaa;
    }
#else
    random_ether_addr(dev->dev_addr);
#endif
   
    netif_start_queue(dev);
    printk("vir_net_open\n");

    printk("Start sram timer\r\n");
    timer_setup(&sram_timer,sram_timer_expires,0);
    mod_timer(&sram_timer,jiffies + msecs_to_jiffies(TIMER_VALUE));


    sram_base = ioremap(SRAM_BASE_ADDRESS,0x100);
    ddr_base = ioremap(DDR_BASE_ADDRESS,0x4000000);

    writeb(VNET_UP, sram_base+VNET_UP_DOWN);

    return 0;
}


int vir_net_release(struct net_device *dev) 
{
    netif_stop_queue(dev);
    printk("vir_net_release\n");

    printk("Exit sram timer\r\n");
    del_timer(&sram_timer);
 
    writeb(VNET_DOWN, sram_base+VNET_UP_DOWN);

    iounmap(sram_base);
    iounmap(ddr_base);
    return 0;
}

void vir_net_rx(struct net_device *pdev, u16 len, unsigned char *buf) 
{
    struct sk_buff *skb;

 
    skb = dev_alloc_skb(len+2);
    if(!skb) {
        printk("gecnet rx: low on mem - packet dropped\n");
        pdev->stats.rx_dropped++;
        return;
    }
    skb_reserve(skb, 2);  
    memcpy(skb_put(skb, len), buf, len);    //skb_put to socket buffer
    
    skb->dev = pdev;
    skb->protocol = eth_type_trans(skb, pdev);
    skb->ip_summed = CHECKSUM_UNNECESSARY; 

    pdev->stats.rx_packets++;
    pdev->stats.rx_bytes += len;

    printk("vir_net_rx\n");
    netif_rx(skb);
    return;
}


void vir_net_hw_tx(char *buf, int len, struct net_device *dev)
{
    struct net_device *dest;


    if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) 
    {
        printk("vir_net: packet too short (%i octets)\n", len);
        return;
    }
    dest = vir_net_devs;



    printk("vir_net_hw_tx\n");
 

}



int vir_net_tx(struct sk_buff *skb, struct net_device *pdev) 
{
    int len;
    char *data;
 //   struct vir_net_priv *priv = (struct vir_net_priv *)pdev->ml_priv;
    if(skb == NULL) 
    {
        printk("net_device = %p, skb = %p\n", pdev, skb);
        return 0;
    }
    
    len = skb->len < ETH_ZLEN ? ETH_ZLEN : skb->len;
    
    data = skb->data;


    pdev->stats.tx_packets++;
    pdev->stats.tx_bytes += skb->len;

    printk("vir_net_tx, pdev = %p\n", pdev);
    return 0; 
}


int vir_net_device_init(struct net_device *pdev) 
{
    
    ether_setup(pdev);
    
    pdev->flags |= IFF_NOARP;

 
 
    return 0;
}


/**/
static const struct net_device_ops vir_net_netdev_ops = 
{
    .ndo_open       = vir_net_open,       //open net ---ifconfig xx up
    .ndo_stop       = vir_net_release,    //close net ---ifconfig xx down
    .ndo_start_xmit = vir_net_tx,   
    .ndo_init       = vir_net_device_init,       //initial net hardware
};


/**/
static void vir_plat_net_release(struct device *pdev) 
{
    printk("vir_plat_net_release, pdev = %p\n", pdev);
}


/**/
static int vir_net_probe(struct platform_device *pdev) 
{
    int result = 0;
    
    vir_net_devs = alloc_etherdev(sizeof(struct net_device));
    vir_net_devs->netdev_ops = &vir_net_netdev_ops;
    strcpy(vir_net_devs->name, "vnet0");

    if ((result = register_netdev(vir_net_devs))) {
        printk("vir_net: error %i registering device \"%s\"\n", result, vir_net_devs->name);
    }
    printk("vir_net_probe, pdev = %p\n", pdev);
    return 0;
}


static int  vir_net_remove(struct platform_device *pdev) 
{

    unregister_netdev(vir_net_devs);

    return 0;
}


static struct platform_device vir_net= 
{
    .name = "vir_net",
    .id   = -1,
    .dev  = {
    .release = vir_plat_net_release,
    },
};


static struct platform_driver vir_net_driver = 
{
    .probe  = vir_net_probe,
    .remove  = vir_net_remove,
    .driver  = {
    .name = "vir_net",    
    .owner = THIS_MODULE,
    },
};


static int __init vir_net_init(void) 
{
    printk("vir_net_init\n");
    platform_device_register(&vir_net);
    return platform_driver_register(&vir_net_driver);
}


static void __exit vir_net_exit(void) 
{
    printk("vir_net_exit\n");
    platform_driver_unregister(&vir_net_driver);
    platform_device_unregister(&vir_net);
}

module_init(vir_net_init);
module_exit(vir_net_exit);
MODULE_LICENSE("GPL");

