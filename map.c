#include"pig_fs.h"

int checkbit(uint8_t number,int x)
{
    return (number>>x)&1U;
}

int pig_find_first_zero_bit(const void *vaddr,unsigned size)
{
    const unsigned short *p=vaddr,*addr=vaddr;
    unsigned short num;

    if(!size)
        return 0;
    
    size>>=4;
    while(*p++==0xffff)
    {
        if(--size==0)
            return (p-addr)<<4;
    }

    num=*--p;
    return ((p-addr)<<4)+ffz(num);
}

uint64_t pig_fs_get_empty_inode(struct super_block* sb)
{
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;


    ssize_t imap_size=disk_sb->blocks_count/8;
    uint8_t *imap= kmalloc(imap_size,GFP_KERNEL);
    if(get_imap(sb,imap,imap_size)!=0)
    {
        kfree(imap);
        return -EFAULT;
    }

    uint64_t empty_ilock_num=pig_find_first_zero_bit(imap,disk_sb->blocks_count/8);
    kfree(imap);
    return empty_ilock_num;
}

int get_imap(struct super_block* sb,uint8_t* imap,ssize_t imap_size)
{
    if(!imap)
    {
        return -EFAULT;
    }
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;

    uint64_t i;
    for(i=disk_sb->imap_block;i<disk_sb->data_block_number&& imap_size!=0;++i)
    {
        struct buffer_head *bh;
        bh=sb_bread(sb,i);

        if(!bh)
        {
            printk(KERN_INFO"bh is empty");
        }
        uint8_t *imap_t=(uint8_t*)bh->b_data;
        if(imap_size>=PIG_BLOCK_SIZE)
        {
            memcpy(imap,imap_t,PIG_BLOCK_SIZE);
            imap_size-=PIG_BLOCK_SIZE;
        }else{
            memcpy(imap,imap_t,imap_size);
            imap_size=0;
        }
       
    } 
    return 0;
}

int get_bmap(struct super_block* sb,uint8_t* bmap,ssize_t bmap_size)
{
    if(!bmap)
    {
        return -EFAULT;
    }
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;

    uint64_t i;
    for(i =disk_sb->bmap_block;i<disk_sb->imap_block && bmap_size !=0;++i)
    {
        struct buffer_head *bh;
        bh=sb_bread(sb,i);
        if(!bh)
        {
            return -EFAULT;
        }
        uint8_t *bmap_t=(uint8_t *)bh->b_data;
        if(bmap_size>=PIG_BLOCK_SIZE)
        {
            memcpy(bmap,bmap_t,PIG_BLOCK_SIZE);
            bmap_size-=PIG_BLOCK_SIZE;
        }else{
            memcpy(bmap,bmap_t,bmap_size);
            bmap_size=0;
        }
        brelse(bh);
    }
    return 0;
}


uint64_t pig_fs_get_empty_block(struct super_block* sb)
{
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;

    uint64_t bmap_empty=disk_sb->blocks_count/8;
    uint8_t *bmap=kmalloc(bmap_empty,GFP_KERNEL);
    if(get_bmap(sb,bmap,bmap_empty)!=0)
    {
        kfree(bmap);
        return -EFAULT;
    }
    uint64_t empty_block_num=pig_find_first_zero_bit(bmap,disk_sb->blocks_count/8);
    kfree(bmap);
    return empty_block_num;
}

int set_and_save_imap(struct super_block* sb,uint64_t inode_num,uint8_t value)
{
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;
    uint64_t block_idx=inode_num/(PIG_BLOCK_SIZE*8)+disk_sb->imap_block;
    uint64_t bit_off=inode_num%(PIG_BLOCK_SIZE*8);

    struct buffer_head* bh;
    bh=sb_bread(sb,block_idx);

    BUG_ON(!bh);
    if(value==1)
    {
        setbit(bh->b_data[bit_off/8],bit_off%8);
    }else if(value==0)
    {
        clearbit(bh->b_data[bit_off/8],bit_off%8);
    }else{
        printk(KERN_ERR"value err\n");
    }
    map_bh(bh,sb,block_idx);
    brelse(bh);
    return 0;
}


int save_bmap(struct super_block* sb,uint8_t* bmap,ssize_t bmap_size)
{
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;
    uint64_t block_idx=disk_sb->bmap_block;
    struct buffer_head* bh;
    bh=sb_bread(sb,block_idx);

    memcpy(bh->b_data,bmap,bmap_size);
    brelse(bh);
    return 0;
}

int set_and_save_bmap(struct super_block* sb,uint64_t block_num,uint8_t value)
{
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;
    uint64_t block_idx=block_num/(PIG_BLOCK_SIZE*8)+disk_sb->bmap_block;
    uint64_t bit_off=block_num%(PIG_BLOCK_SIZE*8);

    struct buffer_head* bh;
    bh=sb_bread(sb,block_idx);

    BUG_ON(!bh);
    if(value==1)
    {
        setbit(bh->b_data[bit_off/8],bit_off%8);
    }else if(value==0)
    {
        clearbit(bh->b_data[bit_off/8],bit_off%8);
    }else{
        printk(KERN_ERR"value err\n");
    }
    map_bh(bh,sb,block_idx);
    brelse(bh);
    return 0;
}