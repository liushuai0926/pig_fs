#include "pig_fs.h"

int save_block(struct super_block *sb,uint64_t block_num,void *buf,ssize_t size)
{
    struct pig_fs_super_block *disk_sb;
    disk_sb=sb->s_fs_info;
    struct buffer_head *bh;
    bh=sb_bread(sb,block_num+disk_sb->data_block_number);

    BUG_ON(!bh);
    memset(bh->b_data,0,PIG_BLOCK_SIZE);
    memcpy(bh->b_data,buf,size);
    brelse(bh);
    return 0;
}

int pig_fs_get_block(struct inode *inode,sector_t block,struct buffer_head *bh,int create)
{
     struct super_block *sb=inode->i_sb;

     if(block>PIG_N_BLOCKS)
     {
         return -ENOSPC;
     }

     struct pig_inode p_inode;
     if(pig_fs_get_inode(sb,inode->i_ino,&p_inode)==-1)
     {
         return -EFAULT;
     }  
     if(p_inode.blocks==0)
     {
         if(alloc_block_for_inode(sb,&p_inode,1))
         {
             return -EFAULT;
         }
     }
     mark_inode_dirty(inode);
     map_bh(bh,sb,p_inode.block[0]);
     return 0; 
}

int alloc_block_for_inode(struct super_block*sb,struct pig_inode * p_inode,ssize_t size)
{
    struct pig_fs_super_block* disk_sb;
    ssize_t bmap_size;
    uint8_t* bmap;
    unsigned int i;

    ssize_t alloc_block=size-p_inode->blocks;
    if(size+p_inode->blocks>PIG_N_BLOCKS)
    {
        return -ENOSPC;
    }

    disk_sb=sb->s_fs_info;
    bmap_size=disk_sb->blocks_count/8;
    bmap=kmalloc(bmap_size,GFP_KERNEL);

    if(get_bmap(sb,bmap,bmap_size))
    {
        kfree(bmap);
        return -EFAULT;
    }

    for(i=0;i<alloc_block;++i)
    {
        uint64_t empty_blk_num=pig_find_first_zero_bit(bmap,disk_sb->blocks_count/8);
        p_inode->block[p_inode->blocks]=empty_blk_num;
        p_inode->blocks++;
        uint64_t bit_off=empty_blk_num%(PIG_BLOCK_SIZE*8);
        setbit(bmap[bit_off/8],bit_off%8);

    }
    save_bmap(sb,bmap,bmap_size);
    save_inode(sb,*p_inode);
    disk_sb->free_blocks-=size;
    kfree(bmap);
    return 0;
}